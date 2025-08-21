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

/* Permission Masks */
#define FWL_PERM_SEC_MASK (0x00FFU)
#define FWL_PERM_NSEC_MASK (0xFF00U)
#define FWL_PERM_PRIV_MASK (0x0F0FU)
#define FWL_PERM_USER_MASK (0xF0F0U)

#define FWL_PERM_WRITE_MASK (0x1111U)
#define FWL_PERM_READ_MASK (0x2222U)
#define FWL_PERM_CACHE_MASK (0x4444U)
#define FWL_PERM_DEBUG_MASK (0x8888U)

#define FWL_PERM_RW_ALL (FWL_PERM_WRITE_MASK | FWL_PERM_READ_MASK | FWL_PERM_CACHE_MASK | FWL_PERM_DEBUG_MASK)
#define FWL_PERM_RO_ALL (FWL_PERM_READ_MASK | FWL_PERM_CACHE_MASK | FWL_PERM_DEBUG_MASK)
#define FWL_PERM_WO_ALL (FWL_PERM_WRITE_MASK | FWL_PERM_CACHE_MASK | FWL_PERM_DEBUG_MASK)

#define FWL_PERM_RO 0b0110011001100110
#define FWL_PERM_RW 0b1111111111111111

#define FWL_ID 66
#define FWL_REGION 0

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

struct tisci_fwl_get_req {
	struct tisci_msg_header hdr;
	uint16_t fwl_id;
	uint16_t region;
	uint32_t n_permission_regs;
} __attribute__ ((__packed__));

struct tisci_fwl {
	struct tisci_msg_header hdr;
	uint16_t fwl_id;
	uint16_t region;
	uint32_t n_permission_regs;
	uint32_t control;
	uint32_t permissions[3U];
	uint64_t start_address;
	uint64_t end_address;
} __attribute__ ((__packed__));

struct tisci_fwl_owner_req {
	struct tisci_msg_header hdr;
	uint16_t fwl_id;
	uint16_t region;
	uint8_t owner_index;
} __attribute__ ((__packed__));

struct tisci_fwl_owner_resp {
	struct tisci_msg_header hdr;
	uint16_t fwl_id;
	uint16_t region;
	uint8_t owner_index;
	uint8_t owner_privid;
	uint16_t owner_permission_bits;
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
	hdr->flags = flags;
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
	}

	fprintf(stderr, "%s: Thread %d verification successful, ret = %d\n",
		__func__, spt->id, ret);


	if (buflen > SEC_PROXY_MAX_MSG_SIZE) {
		fprintf(stderr, "%s: Thread %u message length %zu > max msg size %d\n",
		       __func__, spt->id, buflen, SEC_PROXY_MAX_MSG_SIZE);
		return;
	}


	for (int num_words = buflen / sizeof(uint32_t);
	     num_words;
	     --num_words, data_reg += sizeof(uint32_t), word_data++)
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
		}
	}

	fprintf(stderr, "%s: Thread %d verification successful, ret = %d\n",
		__func__, spt->id, ret);


	for (num_words = SEC_PROXY_MAX_MSG_SIZE / sizeof(uint32_t); num_words; num_words --, data_reg += sizeof(uint32_t), word_data++)
		*word_data = sp_readl(data_reg);
	return 0;
}

void tisci_setup_fwl_get_req(struct tisci_fwl_get_req *req, uint16_t fwl_id, uint16_t region, uint32_t n_permission_regs)
{
	req->fwl_id = fwl_id;
	req->region = region;
	req->n_permission_regs = n_permission_regs;
}

void tisci_setup_fwl_set_req(struct tisci_fwl *req, uint16_t fwl_id, uint16_t region, uint32_t n_permission_regs, uint32_t control, uint32_t permissions[], uint64_t start_address, uint64_t end_address)
{
	tisci_setup_fwl_get_req((struct tisci_fwl_get_req*)req, fwl_id, region, n_permission_regs);
	req->control = control;
	req->permissions[0] = permissions[0];
	req->permissions[1] = permissions[1];
	req->permissions[2] = permissions[2];
	req->start_address = start_address;
	req->end_address = end_address;

	printf("FROM INSIDE _SET_: fwl_id: %d, region: %d, n_perms: %d, control: %x, permissions: %x %x %x, start: %x, end: %x\n",
	       req->fwl_id, req->region, req->n_permission_regs, req->control, req->permissions[0], req->permissions[1], req->permissions[2], req->start_address, req->end_address);
}

void tisci_setup_owner_req(struct tisci_fwl_owner_req *req, uint16_t fwl_id, uint16_t region, uint8_t owner_index)
{
	req->fwl_id = fwl_id;
	req->region = region;
	req->owner_index = owner_index;
}

int main(int argc, char **argv)
{
	if (argc <= 1) {
		printf("Incorrect usage: specify command and its arguments.\n");
		exit(1);
	}
	
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

	tisci_setup_header((struct tisci_msg_header *) buf, 0x0002, (1 << 1));
	tisci_send_msg(buf, sizeof(struct tisci_msg_header));

	memset(buf, 0, sizeof(buf));
	ret = tisci_recv_msg(buf, sizeof(struct tisci_msg_header));

	struct tisci_msg_version_resp *version = (struct tisci_msg_version_resp*)buf;
	printf("Flag: %d, Version: %d.%d.%d\n", version->hdr.flags, version->version, version->abi_major, version->abi_minor);

	uint16_t region, fwl_id;
	uint32_t n_perms, control, perm;
	uint64_t start_address, end_address;
	struct tisci_fwl *fwl_get_resp;

	if (!strcmp(argv[1], "get")) {
		if (argc < 5) {
			printf("Insufficient arguments, exiting\n");
			exit(1);
		}

		fwl_id = atoi(argv[2]);
		region = atoi(argv[3]);
		n_perms = atoi(argv[4]);

		printf("\n========== Current permission configurations ==========\n");
		memset(buf, 0, sizeof(buf));
		tisci_setup_header((struct tisci_msg_header *)buf, 0x9001, (1 << 1));
		tisci_setup_fwl_get_req((struct tisci_fwl_get_req *)buf, fwl_id, region, n_perms);
		tisci_send_msg(buf, sizeof(struct tisci_fwl_get_req));
		memset(buf, 0, sizeof(buf));
		tisci_recv_msg(buf, sizeof(struct tisci_fwl));
		fwl_get_resp = (struct tisci_fwl *)buf;
		printf("FWL ID: %d, Region: %d, n_permission_regs: %d\ncontrol: %d, permissions: %x %x %x, start_address: %x, end_address: %x\n",
		       fwl_get_resp->fwl_id, fwl_get_resp->region, fwl_get_resp->n_permission_regs, fwl_get_resp->control, fwl_get_resp->permissions[0],
		       fwl_get_resp->permissions[1], fwl_get_resp->permissions[2], fwl_get_resp->start_address, fwl_get_resp->end_address);

	} else if (!strcmp(argv[1], "set")) {

		if (argc < 9) {
			printf("Insufficient arguments, exiting...\n");
			exit(1);
		}

		fwl_id = atoi(argv[2]);
		region = atoi(argv[3]);
		char *end;
		n_perms = atoi(argv[4]);
		//control = ((1 << 9) | strtoul(argv[5], &end, 16));
		control = strtoul(argv[5], &end, 16);
		perm = strtoul(argv[6], &end, 16);
		start_address = strtoul(argv[7], &end, 16);
		end_address = strtoul(argv[8], &end, 16);
		
		uint32_t perms[3];
		//		for (int i = 0 ; i < 3; ++i) {
		//	perms[i] = perm;//0xF0F0U & (0x1111U | 0x2222U | 0x3333U | 0x8888U); //(((uint32_t)195U << 16) | 0xFFFFU); //fwl_get_resp->permissions[i];
		// }
		perms[0] = ((0x1 << 16) | perm);
		perms[1] = (100 << 16) | perm;
		perms[2] = ((212 << 16) | perm);

		printf("\n========== Setting permission configurations ==========\n");
		memset(buf, 0, sizeof(buf));
		tisci_setup_header((struct tisci_msg_header *)buf, 0x9000, (1 << 1));
		tisci_setup_fwl_set_req((struct tisci_fwl *)buf, fwl_id, region, n_perms, control, perms, start_address, end_address);
		tisci_send_msg(buf, sizeof(struct tisci_fwl));
		memset(buf, 0, sizeof(buf));
		tisci_recv_msg(buf, sizeof(struct tisci_msg_header));
		struct tisci_msg_header *hdr_ret = (struct tisci_msg_header*)buf;
		printf("Acknowledgement received thus: type: %x, host: %d, seq: %d, flags: %d\n",
		       hdr_ret->type, hdr_ret->host, hdr_ret->seq, hdr_ret->flags);

		printf("\n========== New permission configurations ==========\n");
		memset(buf, 0, sizeof(buf));
		tisci_setup_header((struct tisci_msg_header *)buf, 0x9001, (1 << 1));
		tisci_setup_fwl_get_req((struct tisci_fwl_get_req*)buf, fwl_id, region, n_perms);
		tisci_send_msg(buf, sizeof(struct tisci_fwl_get_req));
		memset(buf, 0, sizeof(buf));
		tisci_recv_msg(buf, sizeof(struct tisci_fwl));
		fwl_get_resp = (struct tisci_fwl *)buf;
		printf("FWL ID: %d, Region: %d, n_permission_regs: %d\ncontrol: %d, permissions: %x %x %x, start_address: %x, end_address: %x\n",
		       fwl_get_resp->fwl_id, fwl_get_resp->region, fwl_get_resp->n_permission_regs, fwl_get_resp->control, fwl_get_resp->permissions[0],
		       fwl_get_resp->permissions[1], fwl_get_resp->permissions[2], fwl_get_resp->start_address, fwl_get_resp->end_address);
	} else if (!strcmp(argv[1], "test")) {

		if (argc < 3) {
			printf("insufficient arguments, exiting...\n");
			exit(1);
		}

		char *end;
		uint32_t addr = strtoul(argv[2], &end, 16);
		uint32_t v = sp_readl(addr);
		printf("%d\n", v);
		sp_writel(addr, atoi(argv[3]));
		v = sp_readl(addr);
		printf("%d\n", v);

	} else if (!strcmp(argv[1], "owner")) {
		if (argc < 5) {
			printf("Insufficent arguments... exiting. \n");
			exit(1);
		}

		fwl_id = atoi(argv[2]);
		region = atoi(argv[3]);
		uint8_t owner_index = atoi(argv[4]);

		printf("\n=========== Setting new owner =============\n");
		memset(buf, 0, sizeof(buf));
		tisci_setup_header((struct tisci_msg_header*)buf, 0x9002U, (1 << 1));
		tisci_setup_owner_req((struct tisci_fwl_owner_req *)buf, fwl_id, region, owner_index);
		tisci_send_msg(buf, sizeof(struct tisci_fwl_owner_req));
		memset(buf, 0, sizeof(buf));
		tisci_recv_msg(buf, sizeof(struct tisci_fwl_owner_resp));
		struct tisci_fwl_owner_resp *owner_resp = (struct tisci_fwl_owner_resp *)buf;
		printf("Flag: %d, FWL ID: %d, Region: %d, Owner_index: %d\nOwner_privid: %d, Owner_permission_bits: %b\n", owner_resp->hdr.flags, owner_resp->fwl_id, owner_resp->region,
		       owner_resp->owner_index, owner_resp->owner_privid, owner_resp->owner_permission_bits);
	} else {
		printf("Incorrect command\n");
		exit(1);
	}

		

	printf("\nDone\n");

	return 0;
}
