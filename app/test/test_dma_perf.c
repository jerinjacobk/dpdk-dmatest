/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <inttypes.h>

#include "test.h"

struct dmadev;

unsigned int dev_reg[10240];
volatile unsigned int *ring;
volatile unsigned int *doorbell;

static void
init_global(void)
{
        ring = &dev_reg[100];
        doorbell = &dev_reg[10000];
}

typedef int (*enqueue_t)(struct dmadev *dev, int vchan, void *src, void *dst, int len, const int flags);
typedef void (*perform_t)(struct dmadev *dev, int vchan);

struct dmadev {
        enqueue_t enqueue;
        perform_t perform;
        char rsv[512];
} __rte_cache_aligned;

static inline void
delay(void)
{
        volatile int k;

        for (k = 0; k < 16; k++) {


      }

}

__rte_noinline
int
hisi_dma_enqueue(struct dmadev *dev, int vchan, void *src, void *dst, int len, const int flags)
{
	delay();

        *ring = 1;

	return 0;
}

__rte_noinline
int
hisi_dma_enqueue_doorbell(struct dmadev *dev, int vchan, void *src, void *dst, int len, const int flags)
{
	delay();

        *ring = 1;

        if (unlikely(flags == 1)) {
                rte_wmb();
                *doorbell = 1;
        }
	return 0;
}

__rte_noinline
void
hisi_dma_perform(struct dmadev *dev, int vchan)
{
        rte_wmb();
        *doorbell = 1;
}

struct dmadev devlist[64];

static void
init_devlist(bool enq_doorbell)
{
        int i;
        for (i = 0; i < 64; i++) {
                devlist[i].enqueue = enq_doorbell ? hisi_dma_enqueue_doorbell : hisi_dma_enqueue;
                devlist[i].perform = hisi_dma_perform;
        }
}

static inline int
dma_enqueue(int dev_id, int vchan, void *src, void *dst, int len, const int flags)
{
        struct dmadev *dev = &devlist[dev_id];
        return dev->enqueue(dev, vchan, src, dst, len, flags);
}

static inline void
dma_perform(int dev_id, int vchan)
{
        struct dmadev *dev = &devlist[dev_id];
        return dev->perform(dev, vchan);
}

#define MAX_LOOPS       900000

__rte_noinline
void test_for_perform_after_multiple_enqueue(const unsigned int burst)
{
        unsigned int i, j;
        init_devlist(false);

	const uint64_t start = rte_get_timer_cycles();
        for (i = 0; i < MAX_LOOPS; i++) {
                for (j = 0; j < burst; j++)
                        (void)dma_enqueue(10, 0, NULL, NULL, 0, 0);
                dma_perform(10, 0);
        }
	const uint64_t end = rte_get_timer_cycles();
	printf("%42s: burst=%d cycles=%f\n", __func__, burst, (float)((double)((end - start)/MAX_LOOPS)));
}

__rte_noinline
void test_for_last_enqueue_issue_doorbell(const unsigned int burst)
{
        unsigned int i, j;
        init_devlist(true);

	const uint64_t start = rte_get_timer_cycles();
        for (i = 0; i < MAX_LOOPS; i++) {
                for (j = 0; j < burst - 1; j++)
                        (void)dma_enqueue(10, 0, NULL, NULL, 0, 0);
                dma_enqueue(10, 0, NULL, NULL, 0, 1);
        }
	const uint64_t end = rte_get_timer_cycles();
	printf("%42s: burst=%d cycles=%f\n", __func__, burst, (float)((double)((end - start)/MAX_LOOPS)));
}

static int
test_dma_perf(void)
{
	int burst;
	printf("lcore=%d Timer running at %5.2fMHz\n", rte_lcore_id(),
				rte_get_timer_hz()/1E6);

        init_global();
	for (burst = 1; burst <=32; burst += 1) {
		test_for_perform_after_multiple_enqueue(burst);
		test_for_last_enqueue_issue_doorbell(burst);
		printf("-------------------------------------------------------------------------------\n");
	}
	return TEST_SUCCESS;
}

REGISTER_TEST_COMMAND(dma_perf_autotest, test_dma_perf);
