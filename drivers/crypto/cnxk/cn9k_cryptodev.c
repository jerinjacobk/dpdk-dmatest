/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_dev.h>
#include <rte_pci.h>

#include "cn9k_cryptodev.h"
#include "cn9k_cryptodev_ops.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_capabilities.h"

#include "roc_api.h"

uint8_t cn9k_cryptodev_driver_id;

static struct rte_pci_id pci_id_cpt_table[] = {
	{
	},
	/* sentinel */
	{
		.device_id = 0
	},
};

static int
cn9k_cpt_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		   struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = rte_socket_id(),
		.private_data_size = sizeof(struct cnxk_cpt_vf)
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *dev;
	struct roc_cpt *roc_cpt;
	struct cnxk_cpt_vf *vf;
	int rc;

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = rte_cryptodev_pmd_create(name, &pci_dev->device, &init_params);
	if (dev == NULL) {
		rc = -ENODEV;
		goto exit;
	}

	/* Get private data space allocated */
	vf = dev->data->dev_private;

	roc_cpt = &vf->cpt;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		roc_cpt->pci_dev = pci_dev;
		rc = roc_cpt_dev_init(roc_cpt);
		if (rc) {
			plt_err("Failed to initialize roc cpt rc=%d", rc);
			goto pmd_destroy;
		}

		rc = cnxk_cpt_eng_grp_add(roc_cpt);
		if (rc) {
			plt_err("Failed to add engine group rc=%d", rc);
			goto dev_fini;
		}
	}

	dev->dev_ops = &cn9k_cpt_ops;
	dev->driver_id = cn9k_cryptodev_driver_id;

	cnxk_cpt_caps_populate(vf);

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			     RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO |
			     RTE_CRYPTODEV_FF_HW_ACCELERATED |
			     RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			     RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			     RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			     RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			     RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			     RTE_CRYPTODEV_FF_SYM_SESSIONLESS |
			     RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED |
			     RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT;

	cn9k_cpt_set_enqdeq_fns(dev);

	return 0;

dev_fini:
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		roc_cpt_dev_fini(roc_cpt);
pmd_destroy:
	rte_cryptodev_pmd_destroy(dev);
exit:
	plt_err("Could not create device (vendor_id: 0x%x device_id: 0x%x)",
		pci_dev->id.vendor_id, pci_dev->id.device_id);
	return rc;
}

static int
cn9k_cpt_pci_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *dev;
	struct cnxk_cpt_vf *vf;
	int ret;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = rte_cryptodev_pmd_get_named_dev(name);
	if (dev == NULL)
		return -ENODEV;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		vf = dev->data->dev_private;
		ret = roc_cpt_dev_fini(&vf->cpt);
		if (ret)
			return ret;
	}

	return rte_cryptodev_pmd_destroy(dev);
}

static struct rte_pci_driver cn9k_cryptodev_pmd = {
	.id_table = pci_id_cpt_table,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn9k_cpt_pci_probe,
	.remove = cn9k_cpt_pci_remove,
};

static struct cryptodev_driver cn9k_cryptodev_drv;

RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_CN9K_PMD, cn9k_cryptodev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(CRYPTODEV_NAME_CN9K_PMD, pci_id_cpt_table);
RTE_PMD_REGISTER_KMOD_DEP(CRYPTODEV_NAME_CN9K_PMD, "vfio-pci");
RTE_PMD_REGISTER_CRYPTO_DRIVER(cn9k_cryptodev_drv, cn9k_cryptodev_pmd.driver,
			       cn9k_cryptodev_driver_id);
