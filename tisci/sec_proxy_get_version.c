#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int seq = 0, fd;
static int warn_user_once = 0;
unsigned mapped_size, page_size, offset_in_page;
void *map_base, *virt_addr;

#define THREAD_TX_ID 15
#define THREAD_RX_ID 14

#define SEC_PROXY_THREAD(base, x) ((base) + (0x1000 * (x)))

#define SEC_PROXY_MAX_MSG_SIZE 60
#define SEC_PROXY_SRC_TARGET_DATA 0x4d000000
#define SEC_PROXY_CFG_SCFG 0x4a400000
#define SEC_PROXY_CFG_RT 0x4a600000

#define SEC_PROXY_DATA_START_OFFS 0x4
#define SEC_PROXY_DATA_END_OFFS 0x3c

#define SEC_PROXY_TX_DATA SEC_PROXY_THREAD(SEC_PROXY_SRC_TARGET_DATA, THREAD_TX_ID)
#define SEC_PROXY_TX_RT SEC_PROXY_THREAD(SEC_PROXY_CFG_RT, THREAD_TX_ID)
#define SEC_PROXY_TX_SCFG SEC_PROXY_THREAD(SEC_PROXY_CFG_SCFG, THREAD_TX_ID)
#define SEC_PROXY_RX_DATA SEC_PROXY_THREAD(SEC_PROXY_SRC_TARGET_DATA, THREAD_RX_ID)
#define SEC_PROXY_RX_RT SEC_PROXY_THREAD(SEC_PROXY_CFG_RT, THREAD_RX_ID)
#define SEC_PROXY_RX_SCFG SEC_PROXY_THREAD(SEC_PROXY_CFG_SCFG, THREAD_RX_ID)

#define RT_THREAD_STATUS			0x0
#define RT_THREAD_STATUS_ERROR_MASK		(1 << 31)
#define RT_THREAD_STATUS_CUR_CNT_MASK		0xff

/* SEC PROXY SCFG THREAD CTRL */
#define SCFG_THREAD_CTRL			0x1000
#define SCFG_THREAD_CTRL_DIR_SHIFT		31
#define SCFG_THREAD_CTRL_DIR_MASK		(1 << 31)

#define NON_ROOT_USER (-13)
#define MEMORY "/dev/mem"

static int k3_sec_proxy_verify_thread(uint32_t dir);

/*
 * TX Thread ID: 15
 */

struct tisci_msg_header {
	uint16_t type;
	uint8_t host;
	uint8_t seq;
	uint32_t flags;
} __attribute__ ((__packed__));

struct tisci_msg_version_resp {
	struct tisci_msg_header hdr;
	char firmware_description[32];
	uint16_t version;
	uint8_t abi_major;
	uint8_t abi_minor;
} __attribute__ ((__packed__));

struct tisci_sec_proxy_thread {
	uint32_t id;
	uintptr_t data;
	uintptr_t scfg;
	uintptr_t rt;
} spts[2];

static void write_reg(int width, uint64_t writeval)
{
	*(volatile uint32_t *)virt_addr = writeval;
}

static uint64_t read_reg(int width)
{
	return *(volatile uint32_t *)virt_addr;
}

static int map_address(off_t target)
{
	unsigned int width = 8 * sizeof(uint64_t);
	uid_t uid = getuid();
	uid_t euid = geteuid();

	/* Ensure that user privilege is proper */
	if (uid || uid != euid) {
		if (!warn_user_once)
			fprintf(stderr, "Missing sudo to access %s?\n", MEMORY);
		warn_user_once = 1;
		return NON_ROOT_USER;
	}

	fd = open(MEMORY, (O_RDWR | O_SYNC));
	if (fd < 0) {
		fprintf(stderr, "Could not open %s!\n", MEMORY);
		return -5;
	}

	mapped_size = page_size = getpagesize();
	offset_in_page = (unsigned)target & (page_size - 1);
	if (offset_in_page + width > page_size) {
		/*
		 * This access spans pages.
		 * Must map two pages to make it possible:
		 */
		mapped_size *= 2;
	}
	map_base = mmap(NULL,
			mapped_size,
			(PROT_READ | PROT_WRITE),
			MAP_SHARED, fd, target & ~(off_t) (page_size - 1));
	if (map_base == MAP_FAILED) {
		fprintf(stderr, "Map fail\n");
		return -1;
	}

	virt_addr = (char *)map_base + offset_in_page;
	return 0;
}

static void unmap_address(void)
{
	if (munmap(map_base, mapped_size) == -1)
		fprintf(stderr, "munmap");
	close(fd);

}

static inline void sp_writel(uintptr_t addr, uint32_t data)
{
	int r = map_address(addr);
	if (r)
		return;
	write_reg(32, data);
	unmap_address();
}

uint32_t sp_readl(uintptr_t addr)
{
	uint32_t v = 0;
	int r;

	r = map_address(addr);
	if (r)
		return 0;
	v = read_reg(32);
	unmap_address();
	return v;
}

void tisci_setup_header(struct tisci_msg_header *hdr, uint16_t type, uint32_t flags)
{
	hdr->type = type;
	hdr->flags = ((1 << 1) | flags);
	hdr->host = 13;
	hdr->seq = seq++;
}

void tisci_send_msg(uint8_t *buf, int buflen)
{
	struct tisci_sec_proxy_thread *spt = &spts[0];
	uintptr_t data_reg = spt->data + SEC_PROXY_DATA_START_OFFS;
	uint32_t *word_data = (uint32_t*)buf;

	int ret = k3_sec_proxy_verify_thread(0);
	if (ret) {
		fprintf(stderr, "%s: Thread %d verification failed. ret = %d\n",
			__func__, spt->id, ret);
		return;
	} else {
		fprintf(stderr, "%s: Thread %d verification successful, ret = %d\n",
			__func__, spt->id, ret);
	}

	if (buflen > SEC_PROXY_MAX_MSG_SIZE) {
		fprintf(stderr, "%s: Thread %u message length %zu > max msg size %d\n",
		       __func__, spt->id, buflen, SEC_PROXY_MAX_MSG_SIZE);
		return;
	}


	for (int num_words = buflen / sizeof(uint32_t); num_words; --num_words, data_reg += sizeof(uint32_t), word_data++)
		sp_writel(data_reg, *word_data);

	int trail_bytes = buflen % sizeof(uint32_t);
	if (trail_bytes) {
		uint32_t data_trail = *word_data;
		data_trail &= 0xFFFFFFFF >> (8 * (sizeof(uint32_t) - trail_bytes));
		sp_writel(data_reg, data_trail);
		data_reg++;
	}

	if (data_reg <= (spt->data + SEC_PROXY_DATA_END_OFFS))
		sp_writel(spt->data + SEC_PROXY_DATA_END_OFFS, 0);
}

static int k3_sec_proxy_verify_thread(uint32_t dir)
{
	struct tisci_sec_proxy_thread *spt = &spts[dir];
	/* Check for any errors already available */
	if (sp_readl(spt->rt + RT_THREAD_STATUS) &
	    RT_THREAD_STATUS_ERROR_MASK) {
		fprintf(stderr, "%s: Thread %d is corrupted, cannot send data.\n",
		       __func__, spt->id);
		return -1;
	}

	/* Make sure thread is configured for right direction */
	if ((sp_readl(spt->scfg + SCFG_THREAD_CTRL)
	    & SCFG_THREAD_CTRL_DIR_MASK) >> SCFG_THREAD_CTRL_DIR_SHIFT != dir) {
		if (dir)
			fprintf(stderr, "%s: Trying to receive data on tx Thread %d\n",
			       __func__, spt->id);
		else
			fprintf(stderr, "%s: Trying to send data on rx Thread %d\n",
			       __func__, spt->id);
		return -1;
	}

	/* Check the message queue before sending/receiving data */
	if (!(sp_readl(spt->rt + RT_THREAD_STATUS) & RT_THREAD_STATUS_CUR_CNT_MASK))
	      return -2;

	return 0;
}

int tisci_recv_msg(uint8_t *buf, int buflen)
{
	struct tisci_sec_proxy_thread *spt = &spts[1];
	uintptr_t data_reg = spt->data + SEC_PROXY_DATA_START_OFFS;
	uint32_t *word_data = (uint32_t*)(uintptr_t)buf;
	int ret = -1, retry = 10000, num_words;
	while (retry-- && ret) {
		ret = k3_sec_proxy_verify_thread(1);
		if ((ret && ret != -2) || !retry) {
			fprintf(stderr, "%s: Thread %d verification failed. ret = %d\n", __func__, THREAD_RX_ID, ret);
			return ret;
		} else {
			fprintf(stderr, "%s: Thread %d verification successful, ret = %d\n",
				__func__, spt->id, ret);
		}
	}

	for (num_words = SEC_PROXY_MAX_MSG_SIZE / sizeof(uint32_t); num_words; num_words --, data_reg += sizeof(uint32_t), word_data++)
		*word_data = sp_readl(data_reg);
	return 0;
}

int main()
{
	uint8_t buf[SEC_PROXY_MAX_MSG_SIZE];
	int ret = 0;

	spts[0].id = THREAD_TX_ID;
	spts[0].data = SEC_PROXY_TX_DATA;
	spts[0].scfg = SEC_PROXY_TX_SCFG;
	spts[0].rt = SEC_PROXY_TX_RT;
	spts[1].id = THREAD_RX_ID;
	spts[1].data = SEC_PROXY_RX_DATA;
	spts[1].scfg = SEC_PROXY_RX_SCFG;
	spts[1].rt = SEC_PROXY_RX_RT;

	memset(buf, 0, sizeof(buf));

	tisci_setup_header((struct tisci_msg_header *) buf, 0x0002, 0);
	tisci_send_msg(buf, sizeof(struct tisci_msg_header));

	memset(buf, 0, sizeof(buf));
	ret = tisci_recv_msg(buf, sizeof(struct tisci_msg_header));

	struct tisci_msg_version_resp *version = (struct tisci_msg_version_resp*)buf;
	printf("Version: %d.%d.%d\n", version->version, version->abi_major, version->abi_minor);

	printf("Done\n");

	return 0;
}
