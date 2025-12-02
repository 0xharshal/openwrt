// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/mdio.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/phylink.h>
#include <linux/regmap.h>

#define RTPCS_SDS_CNT				14
#define RTPCS_PORT_CNT				57

#define RTPCS_SPEED_10				0
#define RTPCS_SPEED_100				1
#define RTPCS_SPEED_1000			2
#define RTPCS_SPEED_10000_LEGACY		3
#define RTPCS_SPEED_10000			4
#define RTPCS_SPEED_2500			5
#define RTPCS_SPEED_5000			6

#define RTPCS_838X_CPU_PORT			28
#define RTPCS_838X_MAC_LINK_DUP_STS		0xa19c
#define RTPCS_838X_MAC_LINK_SPD_STS		0xa190
#define RTPCS_838X_MAC_LINK_STS			0xa188
#define RTPCS_838X_MAC_RX_PAUSE_STS		0xa1a4
#define RTPCS_838X_MAC_TX_PAUSE_STS		0xa1a0

#define RTPCS_839X_CPU_PORT			52
#define RTPCS_839X_MAC_LINK_DUP_STS		0x03b0
#define RTPCS_839X_MAC_LINK_SPD_STS		0x03a0
#define RTPCS_839X_MAC_LINK_STS			0x0390
#define RTPCS_839X_MAC_RX_PAUSE_STS		0x03c0
#define RTPCS_839X_MAC_TX_PAUSE_STS		0x03b8

#define RTPCS_83XX_MAC_LINK_SPD_BITS		2

#define RTPCS_930X_CPU_PORT			28
#define RTPCS_930X_MAC_LINK_DUP_STS		0xcb28
#define RTPCS_930X_MAC_LINK_SPD_STS		0xcb18
#define RTPCS_930X_MAC_LINK_STS			0xcb10
#define RTPCS_930X_MAC_RX_PAUSE_STS		0xcb30
#define RTPCS_930X_MAC_TX_PAUSE_STS		0xcb2c

#define RTPCS_931X_CPU_PORT			56
#define RTPCS_931X_MAC_LINK_DUP_STS		0x0ef0
#define RTPCS_931X_MAC_LINK_SPD_STS		0x0ed0
#define RTPCS_931X_MAC_LINK_STS			0x0ec0
#define RTPCS_931X_MAC_RX_PAUSE_STS		0x0f00
#define RTPCS_931X_MAC_TX_PAUSE_STS		0x0ef8
#define RTPCS_931X_MAC_L2_PORT_CTRL		(0x6000)

#define RTPCS_SDS_POLL_INTERVAL			(1 * HZ)

#define RTPCS_931X_ASDS_PAGE(page) (page)
#define RTPCS_931X_XSGMSDS_PAGE(page) (page + 0x40)
#define RTPCS_931X_XSGMSDS1_PAGE(page) (page + 0x80)
#define RTPCS_931X_DSDS_PAGE(page) (RTPCS_931X_XSGMSDS_PAGE(page))


#define RTPCS_93XX_MAC_LINK_SPD_BITS		4

#define RTL93XX_MODEL_NAME_INFO			(0x0004)
#define RTL93XX_CHIP_INFO			(0x0008)

#define PHY_PAGE_2	2
#define PHY_PAGE_4	4

#define RTL9300_PHY_ID_MASK 0xf0ffffff

/* RTL930X SerDes supports the following modes:
 * 0x02: SGMII		0x04: 1000BX_FIBER	0x05: FIBER100
 * 0x06: QSGMII		0x09: RSGMII		0x0d: USXGMII
 * 0x10: XSGMII		0x12: HISGMII		0x16: 2500Base_X
 * 0x17: RXAUI_LITE	0x19: RXAUI_PLUS	0x1a: 10G Base-R
 * 0x1b: 10GR1000BX_AUTO			0x1f: OFF
 */
#define RTL930X_SDS_MODE_SGMII		0x02
#define RTL930X_SDS_MODE_1000BASEX	0x04
#define RTL930X_SDS_MODE_USXGMII	0x0d
#define RTL930X_SDS_MODE_XGMII		0x10
#define RTL930X_SDS_MODE_2500BASEX	0x16
#define RTL930X_SDS_MODE_10GBASER	0x1a
#define RTL930X_SDS_OFF			0x1f
#define RTL930X_SDS_MASK		0x1f

/* RTL930X SerDes supports two submodes when mode is USXGMII:
 * 0x00: USXGMII (aka USXGMII_SX)
 * 0x02: 10G_QXGMII (aka USXGMII_QX)
 */
#define RTL930X_SDS_SUBMODE_USXGMII_SX	0x0
#define RTL930X_SDS_SUBMODE_USXGMII_QX	0x2

#define RTSDS_930X_PLL_1000		0x1
#define RTSDS_930X_PLL_10000		0x5
#define RTSDS_930X_PLL_2500		0x3
#define RTSDS_930X_PLL_LC		0x3
#define RTSDS_930X_PLL_RING		0x1

/* Registers of the internal SerDes of the 9310 */
#define RTL931X_SERDES_MODE_CTRL		(0x13cc)
#define RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR	(0x13F4)
#define RTL931X_MAC_SERDES_MODE_CTRL(sds)	(0x136C + (((sds) << 2)))
#define RTL931X_FIB_UNIDIR_CTRL			(0x13c4)

#define RTL931X_MAC_FORCE_MODE_CTRL_BASE	(0x0dcc)
// #define RTL931X_MAC_FORCE_MODE_CTRL(port)	(0x0dcc + (((port) << 2)))

#define RTL931X_PER_PORT_MAC_DEBUG0_BASE	(0x601c)
// #define RTL931X_PER_PORT_MAC_DEBUG0(port)	(0x601c + (((port) << 2)))

#define RTL931X_ISR_SERDES_RXIDLE		(0x12f8)
#define RTL931X_SERDES_BC_CTRL			(0x5640)
#define RTL931X_FRC_RXDV_H			(0xf4c)
#define RTL931X_FRC_RXDV_L			(0xf50)

#if 0
// drv_rtl9310_sds2XsgmSds_get
static inline int rtpcs_931x_sds2xsgmii_sds(uint32 sds)
{
	if (sds < 2) return sds;
	return (sds - 1) * 2
}

static int rtpcs_931x_get_analog_sds(int sds)
{
	int map[] = {0, 1, 2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23};
	int back = map[sds];

	return back;
}
#endif

#define RTL_MAX_FIELDS		(30)

enum rtpcs_931x_regfield {
	RTL931X_SMI_FORCE_LINK,
	RTL931X_SMI_FORCE_LINK_EN,
	RTL931X_SMI_SPD_SEL,
	RTL931X_SMI_FORCE_SPD_EN,

	RTL931X_TX_NO_PKT,
	RTL931X_RX_NO_PKT,

	RTL931X_MAC_L2_PORT_TX_EN,
	RTL931X_MAC_L2_PORT_RX_EN,

	// RTL931X_FIB_UNIDIR,

	// RTL931X_SERDES_BC_EN,
	// RTL931X_SERDES_BC_ID,

	RTL931X_MAX_FIELDS
};

const struct reg_field rtpcs_931x_reg_fields[RTL931X_MAX_FIELDS] = {
	[RTL931X_SMI_FORCE_LINK] = REG_FIELD_ID(RTL931X_MAC_FORCE_MODE_CTRL_BASE, 9, 9, 56, 4),
	[RTL931X_SMI_FORCE_LINK_EN] = REG_FIELD_ID(RTL931X_MAC_FORCE_MODE_CTRL_BASE, 0, 0, 56, 4),
	[RTL931X_SMI_SPD_SEL] = REG_FIELD_ID(RTL931X_MAC_FORCE_MODE_CTRL_BASE, 12, 16, 56, 4),
	[RTL931X_SMI_FORCE_SPD_EN] = REG_FIELD_ID(RTL931X_MAC_FORCE_MODE_CTRL_BASE, 3, 3, 56, 4),

	[RTL931X_TX_NO_PKT] = REG_FIELD_ID(RTL931X_PER_PORT_MAC_DEBUG0_BASE, 28, 28, 56, 4),
	[RTL931X_RX_NO_PKT] = REG_FIELD_ID(RTL931X_PER_PORT_MAC_DEBUG0_BASE, 29, 29, 56, 4),

	[RTL931X_MAC_L2_PORT_TX_EN] = REG_FIELD_ID(RTPCS_931X_MAC_L2_PORT_CTRL, 1, 1, 56, 0x80),
	[RTL931X_MAC_L2_PORT_RX_EN] = REG_FIELD_ID(RTPCS_931X_MAC_L2_PORT_CTRL, 0, 0, 56, 0x80),

	// [RTL931X_FIB_UNIDIR] = REG_FIELD_ID(RTL931X_FIB_UNIDIR_CTRL, 0, 0, 56, 0),
	// [RTL931X_SERDES_BC_EN] = REG_FIELD_ID(RTL931X_)
};

enum rtpcs_sds_mode {
	RTPCS_SDS_MODE_OFF = 0,

	/* fiber modes */
	RTPCS_SDS_MODE_1000BASEX,
	RTPCS_SDS_MODE_2500BASEX,
	RTPCS_SDS_MODE_10GBASER,

	/* mii modes */
	RTPCS_SDS_MODE_SGMII,
	RTPCS_SDS_MODE_HISGMII,
	RTPCS_SDS_MODE_QSGMII,
	RTPCS_SDS_MODE_QHSGMII,
	RTPCS_SDS_MODE_XSGMII,

	RTPCS_SDS_MODE_USXGMII_10GSXGMII,
	RTPCS_SDS_MODE_USXGMII_10GDXGMII,
	RTPCS_SDS_MODE_USXGMII_10GQXGMII,
	RTPCS_SDS_MODE_USXGMII_5GSXGMII,
	RTPCS_SDS_MODE_USXGMII_5GDXGMII,
	RTPCS_SDS_MODE_USXGMII_2_5GSXGMII,
};
/*
enum rtpcs_sds_10g_fiber_mode {
	RTPCS_10GMEDIA_NONE = 0,

	RTPCS_10GMEDIA_FIBER_10G,
	RTPCS_10GMEDIA_FIBER_1G,
	RTPCS_10GMEDIA_FIBER_2_5G,
	RTPCS_10GMEDIA_FIBER_100M,
	RTPCS_10GMEDIA_DAC_50CM,
	RTPCS_10GMEDIA_DAC_100CM,
	RTPCS_10GMEDIA_DAC_300CM,
	RTPCS_10GMEDIA_DAC_500CM,
	RTPCS_10GMEDIA_END,
};
*/

struct rtpcs_ctrl {
	struct device *dev;
	struct regmap *map;
	#if 0
	struct regmap_field *rm_fields[RTL931X_MAX_FIELDS];
	#else
	struct regmap_field **rm_fields;
	#endif
	struct mii_bus *bus;
	const struct rtpcs_config *cfg;
	struct rtpcs_link *link[RTPCS_PORT_CNT];
	bool rx_pol_inv[RTPCS_SDS_CNT];
	bool tx_pol_inv[RTPCS_SDS_CNT];
	struct mutex lock;
	struct workqueue_struct *wq;
	struct delayed_work link_check_work;
};

struct rtpcs_link_sts {
	int sts;
	int sts1;
	int latch_sts;
	int latch_sts1;
};
struct rtpcs_link {
	struct rtpcs_ctrl *ctrl;
	struct phylink_pcs pcs;
	int sds;
	int port;
	enum rtpcs_sds_mode sds_mode;
	struct rtpcs_link_sts link_sts;
	bool is_rx_calibrated;
};

struct rtpcs_config {
	int cpu_port;
	int mac_link_dup_sts;
	int mac_link_spd_bits;
	int mac_link_spd_sts;
	int mac_link_sts;
	int mac_rx_pause_sts;
	int mac_tx_pause_sts;
	const struct phylink_pcs_ops *pcs_ops;
	int (*set_autoneg)(struct rtpcs_ctrl *ctrl, int sds, unsigned int neg_mode);
	int (*setup_serdes)(struct rtpcs_ctrl *ctrl, int sds, phy_interface_t mode);
	int (*setup_pcs_serdes)(struct phylink_pcs *pcs, int sds, int port, phy_interface_t mode);
	void (*init_link_check)(struct rtpcs_ctrl *ctrl);
	const struct reg_field *reg_fields;
	const int num_reg_fields;
};

enum rtpcs_sds_cmu_type {
	RTPCS_SDS_CMU_NONE = 0,
	RTPCS_SDS_CMU_LC,
	RTPCS_SDS_CMU_RING,
};
typedef struct {
	u8 page;
	u8 reg;
	u16 data;
} sds_config;

enum rtpcs_9310_dfe_type {
	RTPCS_9310_DFE_VTH,
	RTPCS_9310_DFE_TAP0,
	RTPCS_9310_DFE_TAP1EVEN,
	RTPCS_9310_DFE_TAP1ODD,
	RTPCS_9310_DFE_TAP2EVEN,
	RTPCS_9310_DFE_TAP2ODD,
	RTPCS_9310_DFE_TAP3EVEN,
	RTPCS_9310_DFE_TAP3ODD,
	RTPCS_9310_DFE_TAP4EVEN,
	RTPCS_9310_DFE_TAP4ODD,
	RTPCS_9310_DFE_FGCAL_OFST,
	RTPCS_9310_DFE_END,
};

#define RTPCS_SDS_SYMERR_ALL_MAX	2
#define RTPCS_SDS_SYMERR_CHANNEL_MAX	8
struct rtpcs_symerr {
	u32 ch[RTPCS_SDS_SYMERR_CHANNEL_MAX];
	// unused members
	// u32 all[RTPCS_SDS_SYMERR_ALL_MAX];
	// u32 latch_blk_lock;
	// u32 latch_hiber;
	// u32 ber;
	// u32 blk_err;
};

struct rtpcs_9310_dfe {
	u32 coef_num;
	u32 end_bit, start_bit;
	u32 sign_bit;
	s32 val;
	enum rtpcs_9310_dfe_type type;
};

static int rtpcs_sds_to_mmd(int sds_page, int sds_regnum)
{
	return (sds_page << 8) + sds_regnum;
}

static int rtpcs_sds_read(struct rtpcs_ctrl *ctrl, int sds, int page, int regnum)
{
	int mmd_regnum = rtpcs_sds_to_mmd(page, regnum);

	return mdiobus_c45_read(ctrl->bus, sds, MDIO_MMD_VEND1, mmd_regnum);
}

static int rtpcs_sds_read_bits(struct rtpcs_ctrl *ctrl, int sds, int page,
			       int regnum, int bithigh, int bitlow)
{
	int mask, val;

	WARN_ON(bithigh < bitlow);

	mask = GENMASK(bithigh, bitlow);
	val = rtpcs_sds_read(ctrl, sds, page, regnum);
	if (val < 0)
		return val;

	return (val & mask) >> bitlow;
}

static int rtpcs_sds_write(struct rtpcs_ctrl *ctrl, int sds, int page, int regnum, u16 value)
{
	int mmd_regnum = rtpcs_sds_to_mmd(page, regnum);

	return mdiobus_c45_write(ctrl->bus, sds, MDIO_MMD_VEND1, mmd_regnum, value);
}

static int rtpcs_sds_write_bits(struct rtpcs_ctrl *ctrl, int sds, int page,
				int regnum, int bithigh, int bitlow, u16 value)
{
	int mask, reg;

	WARN_ON(bithigh < bitlow);

	mask = GENMASK(bithigh, bitlow);
	reg = rtpcs_sds_read(ctrl, sds, page, regnum);
	if (reg < 0)
		return reg;

	reg = (reg & ~mask);
	reg |= (value << bitlow) & mask;

	return rtpcs_sds_write(ctrl, sds, page, regnum, reg);
}

static int rtpcs_sds_modify(struct rtpcs_ctrl *ctrl, int sds, int page, int regnum,
			    u16 mask, u16 set)
{
	int mmd_regnum = rtpcs_sds_to_mmd(page, regnum);

	return mdiobus_c45_modify(ctrl->bus, sds, MDIO_MMD_VEND1, mmd_regnum,
				  mask, set);
}

static int rtpcs_regmap_read_bits(struct rtpcs_ctrl *ctrl, int base, int bithigh, int bitlow)
{
	int offset = base + (bitlow / 32) * 4;
	int bits = bithigh + 1 - bitlow;
	int shift = bitlow % 32;
	int value;

	regmap_read(ctrl->map, offset, &value);
	value = (value >> shift) & (BIT(bits) - 1);

	return value;
}

#if 0
static int rtpcs_regmap_read_masked_bypassed(struct rtpcs_ctrl *ctrl, int reg, unsigned int mask)
{
	unsigned int low;
	unsigned int high;
	struct reg_field r_field = {0};
	struct regmap_field *p_regmap_field = NULL;

	if (mask == 0)
		return -EINVAL;

	low = __builtin_ctz(mask);
	high = (sizeof(unsigned int) * 8 - 1) - __builtin_clz(mask);
	r_field = REG_FIELD(reg, low, high);
	p_regmap_field = devm_regmap_field_alloc(ctrl->dev, )
	return FIELD_GET(mask, ctrl->map->reg_base + reg);
free:
	devm_regmap_field_free(ctrl->dev, )
}
#endif

static struct rtpcs_link *rtpcs_phylink_pcs_to_link(struct phylink_pcs *pcs)
{
	return container_of(pcs, struct rtpcs_link, pcs);
}

/* Variant-specific functions */

/* RTL930X */

/* The access registers for SDS_MODE_SEL and the LSB for each SDS within */
u16 rtpcs_930x_sds_regs[] = { 0x0194, 0x0194, 0x0194, 0x0194, 0x02a0, 0x02a0, 0x02a0, 0x02a0,
			      0x02A4, 0x02A4, 0x0198, 0x0198 };
u8  rtpcs_930x_sds_lsb[]  = { 0, 6, 12, 18, 0, 6, 12, 18, 0, 6, 0, 6};

u16 rtpcs_930x_sds_submode_regs[] = { 0x1cc, 0x1cc, 0x2d8, 0x2d8, 0x2d8, 0x2d8,
				      0x2d8, 0x2d8};
u8  rtpcs_930x_sds_submode_lsb[]  = { 0, 5, 0, 5, 10, 15, 20, 25 };

static void rtpcs_930x_sds_set(struct rtpcs_ctrl *ctrl, int sds_num, u32 mode)
{
	pr_info("%s %d\n", __func__, mode);
	if (sds_num < 0 || sds_num > 11) {
		pr_err("Wrong SerDes number: %d\n", sds_num);
		return;
	}

	regmap_write_bits(ctrl->map, rtpcs_930x_sds_regs[sds_num],
			  RTL930X_SDS_MASK << rtpcs_930x_sds_lsb[sds_num],
			  mode << rtpcs_930x_sds_lsb[sds_num]);
	mdelay(10);
}

__attribute__((unused))
static u32 rtpcs_930x_sds_mode_get(struct rtpcs_ctrl *ctrl, int sds_num)
{
	u32 v;

	if (sds_num < 0 || sds_num > 11) {
		pr_err("Wrong SerDes number: %d\n", sds_num);
		return 0;
	}

	regmap_read(ctrl->map, rtpcs_930x_sds_regs[sds_num], &v);
	v >>= rtpcs_930x_sds_lsb[sds_num];

	return v & RTL930X_SDS_MASK;
}

__attribute__((unused))
static u32 rtpcs_930x_sds_submode_get(struct rtpcs_ctrl *ctrl, int sds_num)
{
	u32 v;

	if (sds_num < 2 || sds_num > 9) {
		pr_err("%s: unsupported SerDes %d\n", __func__, sds_num);
		return 0;
	}

	regmap_read(ctrl->map, rtpcs_930x_sds_submode_regs[sds_num], &v);
	v >>= rtpcs_930x_sds_submode_lsb[sds_num];

	return v & RTL930X_SDS_MASK;
}

static void rtpcs_930x_sds_submode_set(struct rtpcs_ctrl *ctrl, int sds,
				       u32 submode)
{
	if (sds < 2 || sds > 9) {
		pr_err("%s: submode unsupported on serdes %d\n", __func__, sds);
		return;
	}

	if (submode != RTL930X_SDS_SUBMODE_USXGMII_SX &&
	    submode != RTL930X_SDS_SUBMODE_USXGMII_QX) {
		pr_err("%s: unsupported submode 0x%x\n", __func__, submode);
	}

	regmap_write_bits(ctrl->map, rtpcs_930x_sds_submode_regs[sds - 2],
			  RTL930X_SDS_MASK << rtpcs_930x_sds_submode_lsb[sds - 2],
			  submode << rtpcs_930x_sds_submode_lsb[sds - 2]);
}

static void rtpcs_930x_sds_rx_reset(struct rtpcs_ctrl *ctrl, int sds_num,
				    phy_interface_t phy_if)
{
	int page = 0x2e; /* 10GR and USXGMII */

	if (phy_if == PHY_INTERFACE_MODE_1000BASEX)
		page = 0x24;

	rtpcs_sds_write_bits(ctrl, sds_num, page, 0x15, 4, 4, 0x1);
	mdelay(5);
	rtpcs_sds_write_bits(ctrl, sds_num, page, 0x15, 4, 4, 0x0);
}

static void rtpcs_930x_sds_get_pll_data(struct rtpcs_ctrl *ctrl, int sds,
					int *pll, int *speed)
{
	int sbit, pbit = sds & 1 ? 6 : 4;
	int base_sds = sds & ~1;

	/*
	 * PLL data is shared between adjacent SerDes in the even lane. Each SerDes defines
	 * what PLL it needs (ring or LC) while the PLL itself stores the current speed.
	 */

	*pll = rtpcs_sds_read_bits(ctrl, base_sds, 0x20, 0x12, pbit + 1, pbit);
	sbit = *pll == RTSDS_930X_PLL_LC ? 8 : 12;
	*speed = rtpcs_sds_read_bits(ctrl, base_sds, 0x20, 0x12, sbit + 3, sbit);
}

static int rtpcs_930x_sds_set_pll_data(struct rtpcs_ctrl *ctrl, int sds,
				       int pll, int speed)
{
	int sbit = pll == RTSDS_930X_PLL_LC ? 8 : 12;
	int pbit = sds & 1 ? 6 : 4;
	int base_sds = sds & ~1;

	if ((speed != RTSDS_930X_PLL_1000) &&
	    (speed != RTSDS_930X_PLL_2500) &&
	    (speed != RTSDS_930X_PLL_10000))
		return -EINVAL;

	if ((pll != RTSDS_930X_PLL_RING) && (pll != RTSDS_930X_PLL_LC))
		return -EINVAL;

	if ((pll == RTSDS_930X_PLL_RING) && (speed == RTSDS_930X_PLL_10000))
		return -EINVAL;

	/*
	 * A SerDes clock can either be taken from the low speed ring PLL or the high speed
	 * LC PLL. As it is unclear if disabling PLLs has any positive or negative effect,
	 * always activate both.
	 */

	rtpcs_sds_write_bits(ctrl, base_sds, 0x20, 0x12, 3, 0, 0xf);
	rtpcs_sds_write_bits(ctrl, base_sds, 0x20, 0x12, pbit + 1, pbit, pll);
	rtpcs_sds_write_bits(ctrl, base_sds, 0x20, 0x12, sbit + 3, sbit, speed);

	return 0;
}

static void rtpcs_930x_sds_reset_cmu(struct rtpcs_ctrl *ctrl, int sds)
{
	int reset_sequence[4] = { 3, 2, 3, 1 };
	int base_sds = sds & ~1;
	int pll, speed, i, bit;

	/*
	 * After the PLL speed has changed, the CMU must take over the new values. The models
	 * of the Otto platform have different reset sequences. Luckily it always boils down
	 * to flipping two bits in a special sequence.
	 */

	rtpcs_930x_sds_get_pll_data(ctrl, sds, &pll, &speed);
	bit = pll == RTSDS_930X_PLL_LC ? 2 : 0;

	for (i = 0; i < ARRAY_SIZE(reset_sequence); i++)
		rtpcs_sds_write_bits(ctrl, base_sds, 0x21, 0x0b, bit + 1, bit,
				     reset_sequence[i]);
}

static int rtpcs_930x_sds_wait_clock_ready(struct rtpcs_ctrl *ctrl, int sds)
{
	int i, base_sds = sds & ~1, ready, ready_cnt = 0, bit = (sds & 1) + 4;

	/*
	 * While reconfiguring a SerDes it might take some time until its clock is in sync with
	 * the PLL. During that timespan the ready signal might toggle randomly. According to
	 * GPL sources it is enough to verify that 3 consecutive clock ready checks say "ok".
	 */

	for (i = 0; i < 20; i++) {
		usleep_range(10000, 15000);

		rtpcs_sds_write(ctrl, base_sds, 0x1f, 0x02, 53);
		ready = rtpcs_sds_read_bits(ctrl, base_sds, 0x1f, 0x14, bit, bit);

		ready_cnt = ready ? ready_cnt + 1 : 0;
		if (ready_cnt >= 3)
			return 0;
	}

	return -EBUSY;
}

static int rtpcs_930x_sds_get_internal_mode(struct rtpcs_ctrl *ctrl, int sds)
{
	return rtpcs_sds_read_bits(ctrl, sds, 0x1f, 0x09, 11, 7);
}

static void rtpcs_930x_sds_set_internal_mode(struct rtpcs_ctrl *ctrl, int sds,
					     int mode)
{
	rtpcs_sds_write_bits(ctrl, sds, 0x1f, 0x09, 6, 6, 0x1); /* Force mode enable */
	rtpcs_sds_write_bits(ctrl, sds, 0x1f, 0x09, 11, 7, mode);
}

static void rtpcs_930x_sds_set_power(struct rtpcs_ctrl *ctrl, int sds, bool on)
{
	int power_down = on ? 0x0 : 0x3;
	int rx_enable = on ? 0x3 : 0x1;

	rtpcs_sds_write_bits(ctrl, sds, 0x20, 0x00, 7, 6, power_down);
	rtpcs_sds_write_bits(ctrl, sds, 0x20, 0x00, 5, 4, rx_enable);
}

static void rtpcs_930x_sds_reconfigure_pll(struct rtpcs_ctrl *ctrl, int sds, int pll)
{
	int mode, tmp, speed;

	mode = rtpcs_930x_sds_get_internal_mode(ctrl, sds);
	rtpcs_930x_sds_get_pll_data(ctrl, sds, &tmp, &speed);

	rtpcs_930x_sds_set_power(ctrl, sds, false);
	rtpcs_930x_sds_set_internal_mode(ctrl, sds, RTL930X_SDS_OFF);

	rtpcs_930x_sds_set_pll_data(ctrl, sds, pll, speed);
	rtpcs_930x_sds_reset_cmu(ctrl, sds);

	rtpcs_930x_sds_set_internal_mode(ctrl, sds, mode);
	if (rtpcs_930x_sds_wait_clock_ready(ctrl, sds))
		pr_err("%s: SDS %d could not sync clock\n", __func__, sds);

	rtpcs_930x_sds_set_power(ctrl, sds, true);
}

static int rtpcs_930x_sds_config_pll(struct rtpcs_ctrl *ctrl, int sds,
				     phy_interface_t interface)
{
	int neighbor_speed, neighbor_mode, neighbor_pll, neighbor = sds ^ 1;
	bool speed_changed = true;
	int pll, speed;

	/*
	 * A SerDes pair on the RTL930x is driven by two PLLs. A low speed ring PLL can generate
	 * signals of 1.25G and 3.125G for link speeds of 1G/2.5G. A high speed LC PLL can
	 * additionally generate a 10.3125G signal for 10G speeds. To drive the pair at different
	 * speeds each SerDes must use its own PLL. But what if the SerDess attached to the ring
	 * PLL suddenly needs 10G but the LC PLL is running at 1G? To avoid reconfiguring the
	 * "partner" SerDes we must choose wisely what assignment serves the current needs. The
	 * logic boils down to the following rules:
	 *
	 * - Use ring PLL for slow 1G speeds
	 * - Use LC PLL for fast 10G speeds
	 * - For 2.5G prefer ring over LC PLL
	 *
	 * However, when we want to configure 10G speed while the other SerDes is already using
	 * the LC PLL for a slower speed, there is no way to avoid reconfiguration. Note that
	 * this can even happen when the other SerDes is not actually in use, because changing
	 * the state of a SerDes back to RTL930X_SDS_OFF is not (yet) implemented.
	 */

	neighbor_mode = rtpcs_930x_sds_get_internal_mode(ctrl, neighbor);
	rtpcs_930x_sds_get_pll_data(ctrl, neighbor, &neighbor_pll, &neighbor_speed);

	if ((interface == PHY_INTERFACE_MODE_1000BASEX) ||
	    (interface == PHY_INTERFACE_MODE_SGMII))
		speed = RTSDS_930X_PLL_1000;
	else if (interface == PHY_INTERFACE_MODE_2500BASEX)
		speed = RTSDS_930X_PLL_2500;
	else if (interface == PHY_INTERFACE_MODE_10GBASER)
		speed = RTSDS_930X_PLL_10000;
	else
		return -ENOTSUPP;

	if (!neighbor_mode)
		pll = speed == RTSDS_930X_PLL_10000 ? RTSDS_930X_PLL_LC : RTSDS_930X_PLL_RING;
	else if (speed == neighbor_speed) {
		speed_changed = false;
		pll = neighbor_pll;
	} else if (neighbor_pll == RTSDS_930X_PLL_RING)
		pll = RTSDS_930X_PLL_LC;
	else if (speed == RTSDS_930X_PLL_10000) {
		pr_info("%s: SDS %d needs LC PLL, reconfigure SDS %d to use ring PLL\n",
			__func__, sds, neighbor);
		rtpcs_930x_sds_reconfigure_pll(ctrl, neighbor, RTSDS_930X_PLL_RING);
		pll = RTSDS_930X_PLL_LC;
	} else
		pll = RTSDS_930X_PLL_RING;

	rtpcs_930x_sds_set_pll_data(ctrl, sds, pll, speed);

	if (speed_changed)
		rtpcs_930x_sds_reset_cmu(ctrl, sds);

	pr_info("%s: SDS %d using %s PLL for %s\n", __func__, sds,
		pll == RTSDS_930X_PLL_LC ? "LC" : "ring", phy_modes(interface));

	return 0;
}

static void rtpcs_930x_sds_reset_state_machine(struct rtpcs_ctrl *ctrl, int sds)
{
	rtpcs_sds_write_bits(ctrl, sds, 0x06, 0x02, 12, 12, 0x01); /* SM_RESET bit */
	usleep_range(10000, 20000);
	rtpcs_sds_write_bits(ctrl, sds, 0x06, 0x02, 12, 12, 0x00);
	usleep_range(10000, 20000);
}

static int rtpcs_930x_sds_init_state_machine(struct rtpcs_ctrl *ctrl, int sds,
					     phy_interface_t interface)
{
	int loopback, link, cnt = 20, ret = -EBUSY;

	if (interface != PHY_INTERFACE_MODE_10GBASER)
		return 0;
	/*
	 * After a SerDes mode change it takes some time until the frontend state machine
	 * works properly for 10G. To verify operation readyness run a connection check via
	 * loopback.
	 */
	loopback = rtpcs_sds_read_bits(ctrl, sds, 0x06, 0x01, 2, 2); /* CFG_AFE_LPK bit */
	rtpcs_sds_write_bits(ctrl, sds, 0x06, 0x01, 2, 2, 0x01);

	while (cnt-- && ret) {
		rtpcs_930x_sds_reset_state_machine(ctrl, sds);
		link = rtpcs_sds_read_bits(ctrl, sds, 0x05, 0x00, 12, 12); /* 10G link state (latched) */
		link = rtpcs_sds_read_bits(ctrl, sds, 0x05, 0x00, 12, 12);
		if (link)
			ret = 0;
	}

	rtpcs_sds_write_bits(ctrl, sds, 0x06, 0x01, 2, 2, loopback);
	rtpcs_930x_sds_reset_state_machine(ctrl, sds);

	return ret;
}

static void rtpcs_930x_sds_force_mode(struct rtpcs_ctrl *ctrl, int sds,
				      phy_interface_t interface)
{
	int mode;

	/*
	 * TODO: Usually one would expect that it is enough to modify the SDS_MODE_SEL_*
	 * registers (lets call it MAC setup). It seems as if this complex sequence is only
	 * needed for modes that cannot be set by the SoC itself. Additionally it is unclear
	 * if this sequence should quit early in case of errors.
	 */

	switch (interface) {
	case PHY_INTERFACE_MODE_SGMII:
		mode = RTL930X_SDS_MODE_SGMII;
		break;
	case PHY_INTERFACE_MODE_1000BASEX:
		mode = RTL930X_SDS_MODE_1000BASEX;
		break;
	case PHY_INTERFACE_MODE_2500BASEX:
		mode = RTL930X_SDS_MODE_2500BASEX;
		break;
	case PHY_INTERFACE_MODE_10GBASER:
		mode = RTL930X_SDS_MODE_10GBASER;
		break;
	case PHY_INTERFACE_MODE_NA:
		mode = RTL930X_SDS_OFF;
		break;
	default:
		pr_err("%s: SDS %d does not support %s\n", __func__, sds, phy_modes(interface));
		return;
	}

	rtpcs_930x_sds_set_power(ctrl, sds, false);
	rtpcs_930x_sds_set_internal_mode(ctrl, sds, RTL930X_SDS_OFF);
	if (interface == PHY_INTERFACE_MODE_NA)
		return;

	if (rtpcs_930x_sds_config_pll(ctrl, sds, interface))
		pr_err("%s: SDS %d could not configure PLL for %s\n", __func__,
			sds, phy_modes(interface));

	rtpcs_930x_sds_set_internal_mode(ctrl, sds, mode);
	if (rtpcs_930x_sds_wait_clock_ready(ctrl, sds))
		pr_err("%s: SDS %d could not sync clock\n", __func__, sds);

	if (rtpcs_930x_sds_init_state_machine(ctrl, sds, interface))
		pr_err("%s: SDS %d could not reset state machine\n", __func__, sds);

	rtpcs_930x_sds_set_power(ctrl, sds, true);
	rtpcs_930x_sds_rx_reset(ctrl, sds, interface);
}

static void rtpcs_930x_sds_mode_set(struct rtpcs_ctrl *ctrl, int sds,
				    phy_interface_t phy_mode)
{
	u32 mode;
	u32 submode;

	if (sds < 0 || sds > 11) {
		pr_err("%s: invalid SerDes number: %d\n", __func__, sds);
		return;
	}

	switch(phy_mode) {
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_10GBASER:
		rtpcs_930x_sds_force_mode(ctrl, sds, phy_mode);
		return;
	case PHY_INTERFACE_MODE_10G_QXGMII:
		mode = RTL930X_SDS_MODE_USXGMII;
		submode = RTL930X_SDS_SUBMODE_USXGMII_QX;
		break;
	default:
		pr_warn("%s: unsupported mode %s\n", __func__, phy_modes(phy_mode));
		return;
	}

	/* SerDes off first. */
	rtpcs_930x_sds_set(ctrl, sds, RTL930X_SDS_OFF);

	/* Set the mode. */
	rtpcs_930x_sds_set(ctrl, sds, mode);

	/* Set the submode if needed. */
	if (phy_mode == PHY_INTERFACE_MODE_10G_QXGMII) {
		rtpcs_930x_sds_submode_set(ctrl, sds, submode);
	}
}


static void rtpcs_930x_sds_tx_config(struct rtpcs_ctrl *ctrl, int sds,
				     phy_interface_t phy_if)
{
	/* parameters: rtl9303_80G_txParam_s2 */
	int impedance = 0x8;
	int pre_amp = 0x2;
	int main_amp = 0x9;
	int post_amp = 0x2;
	int pre_en = 0x1;
	int post_en = 0x1;
	int page;

	switch(phy_if) {
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_SGMII:
		pre_amp = 0x1;
		main_amp = 0x9;
		post_amp = 0x1;
		page = 0x25;
		break;
	case PHY_INTERFACE_MODE_2500BASEX:
		pre_amp = 0;
		post_amp = 0x8;
		pre_en = 0;
		page = 0x29;
		break;
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_USXGMII:
	case PHY_INTERFACE_MODE_10G_QXGMII:
	case PHY_INTERFACE_MODE_XGMII:
		pre_en = 0;
		pre_amp = 0;
		main_amp = 0x10;
		post_amp = 0;
		post_en	= 0;
		page = 0x2f;
		break;
	default:
		pr_err("%s: unsupported PHY mode\n", __func__);
		return;
	}

	rtpcs_sds_write_bits(ctrl, sds, page, 0x01, 15, 11, pre_amp);
	rtpcs_sds_write_bits(ctrl, sds, page, 0x06,  4,  0, post_amp);
	rtpcs_sds_write_bits(ctrl, sds, page, 0x07,  0,  0, pre_en);
	rtpcs_sds_write_bits(ctrl, sds, page, 0x07,  3,  3, post_en);
	rtpcs_sds_write_bits(ctrl, sds, page, 0x07,  8,  4, main_amp);
	rtpcs_sds_write_bits(ctrl, sds, page, 0x18, 15, 12, impedance);
}

/* Wait for clock ready, this assumes the SerDes is in XGMII mode
 * timeout is in ms
 */
__attribute__((unused))
static int rtpcs_930x_sds_clock_wait(struct rtpcs_ctrl *ctrl, int timeout)
{
	u32 v;
	unsigned long start = jiffies;

	do {
		rtpcs_sds_write_bits(ctrl, 2, 0x1f, 0x2, 15, 0, 53);
		v = rtpcs_sds_read_bits(ctrl, 2, 0x1f, 20, 5, 4);
		if (v == 3)
			return 0;
	} while (jiffies < start + (HZ / 1000) * timeout);

	return 1;
}

__attribute__((unused))
static void rtpcs_930x_sds_rxcal_dcvs_manual(struct rtpcs_ctrl *ctrl, u32 sds_num,
					     u32 dcvs_id, bool manual, u32 dvcs_list[])
{
	if (manual) {
		switch(dcvs_id) {
		case 0:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 14, 14, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x03,  5,  5, dvcs_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x03,  4,  0, dvcs_list[1]);
			break;
		case 1:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 13, 13, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1d, 15, 15, dvcs_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1d, 14, 11, dvcs_list[1]);
			break;
		case 2:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 12, 12, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1d, 10, 10, dvcs_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1d,  9,  6, dvcs_list[1]);
			break;
		case 3:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 11, 11, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1d,  5,  5, dvcs_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1d,  4,  1, dvcs_list[1]);
			break;
		case 4:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x01, 15, 15, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x11, 10, 10, dvcs_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x11,  9,  6, dvcs_list[1]);
			break;
		case 5:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x02, 11, 11, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x11,  4,  4, dvcs_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x11,  3,  0, dvcs_list[1]);
			break;
		default:
			break;
		}
	} else {
		switch(dcvs_id) {
		case 0:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 14, 14, 0x0);
			break;
		case 1:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 13, 13, 0x0);
			break;
		case 2:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 12, 12, 0x0);
			break;
		case 3:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x1e, 11, 11, 0x0);
			break;
		case 4:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x01, 15, 15, 0x0);
			break;
		case 5:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x02, 11, 11, 0x0);
			break;
		default:
			break;
		}
		mdelay(1);
	}
}

__attribute__((unused))
static void rtpcs_930x_sds_rxcal_dcvs_get(struct rtpcs_ctrl *ctrl, u32 sds_num,
					  u32 dcvs_id, u32 dcvs_list[])
{
	u32 dcvs_sign_out = 0, dcvs_coef_bin = 0;
	bool dcvs_manual;

	if (!(sds_num % 2))
		rtpcs_sds_write(ctrl, sds_num, 0x1f, 0x2, 0x2f);
	else
		rtpcs_sds_write(ctrl, sds_num - 1, 0x1f, 0x2, 0x31);

	/* ##Page0x2E, Reg0x15[9], REG0_RX_EN_TEST=[1] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x15, 9, 9, 0x1);

	/* ##Page0x21, Reg0x06[11 6], REG0_RX_DEBUG_SEL=[1 0 x x x x] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x21, 0x06, 11, 6, 0x20);

	switch(dcvs_id) {
	case 0:
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0x22);
		mdelay(1);

		/* ##DCVS0 Read Out */
		dcvs_sign_out = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  4,  4);
		dcvs_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  3,  0);
		dcvs_manual = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x1e, 14, 14);
		break;

	case 1:
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0x23);
		mdelay(1);

		/* ##DCVS0 Read Out */
		dcvs_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  4,  4);
		dcvs_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  3,  0);
		dcvs_manual = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x1e, 13, 13);
		break;

	case 2:
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0x24);
		mdelay(1);

		/* ##DCVS0 Read Out */
		dcvs_sign_out = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  4,  4);
		dcvs_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  3,  0);
		dcvs_manual = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x1e, 12, 12);
		break;
	case 3:
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0x25);
		mdelay(1);

		/* ##DCVS0 Read Out */
		dcvs_sign_out = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  4,  4);
		dcvs_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  3,  0);
		dcvs_manual   = rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x1e, 11, 11);
		break;

	case 4:
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0x2c);
		mdelay(1);

		/* ##DCVS0 Read Out */
		dcvs_sign_out = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  4,  4);
		dcvs_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  3,  0);
		dcvs_manual = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x01, 15, 15);
		break;

	case 5:
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0x2d);
		mdelay(1);

		/* ##DCVS0 Read Out */
		dcvs_sign_out = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  4,  4);
		dcvs_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14,  3,  0);
		dcvs_manual   = rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x02, 11, 11);
		break;

	default:
		break;
	}

	if (dcvs_sign_out)
		pr_info("%s DCVS %u Sign: -", __func__, dcvs_id);
	else
		pr_info("%s DCVS %u Sign: +", __func__, dcvs_id);

	pr_info("DCVS %u even coefficient = %u", dcvs_id, dcvs_coef_bin);
	pr_info("DCVS %u manual = %u", dcvs_id, dcvs_manual);

	dcvs_list[0] = dcvs_sign_out;
	dcvs_list[1] = dcvs_coef_bin;
}

static void rtpcs_930x_sds_rxcal_leq_manual(struct rtpcs_ctrl *ctrl, u32 sds_num,
					    bool manual, u32 leq_gray)
{
	if (manual) {
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x18, 15, 15, 0x1);
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x16, 14, 10, leq_gray);
	} else {
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x18, 15, 15, 0x0);
		mdelay(100);
	}
}

static void rtpcs_930x_sds_rxcal_leq_offset_manual(struct rtpcs_ctrl *ctrl,
						   u32 sds_num, bool manual,
						   u32 offset)
{
	if (manual) {
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x17, 6, 2, offset);
	} else {
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x17, 6, 2, offset);
		mdelay(1);
	}
}

#define GRAY_BITS 5
static u32 rtpcs_930x_sds_rxcal_gray_to_binary(u32 gray_code)
{
	int i, j, m;
	u32 g[GRAY_BITS];
	u32 c[GRAY_BITS];
	u32 leq_binary = 0;

	for(i = 0; i < GRAY_BITS; i++)
		g[i] = (gray_code & BIT(i)) >> i;

	m = GRAY_BITS - 1;

	c[m] = g[m];

	for(i = 0; i < m; i++) {
		c[i] = g[i];
		for(j  = i + 1; j < GRAY_BITS; j++)
			c[i] = c[i] ^ g[j];
	}

	for(i = 0; i < GRAY_BITS; i++)
		leq_binary += c[i] << i;

	return leq_binary;
}

static u32 rtpcs_930x_sds_rxcal_leq_read(struct rtpcs_ctrl *ctrl, int sds_num)
{
	u32 leq_gray, leq_bin;
	bool leq_manual;

	if (!(sds_num % 2))
		rtpcs_sds_write(ctrl, sds_num, 0x1f, 0x2, 0x2f);
	else
		rtpcs_sds_write(ctrl, sds_num - 1, 0x1f, 0x2, 0x31);

	/* ##Page0x2E, Reg0x15[9], REG0_RX_EN_TEST=[1] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x15, 9, 9, 0x1);

	/* ##Page0x21, Reg0x06[11 6], REG0_RX_DEBUG_SEL=[0 1 x x x x] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x21, 0x06, 11, 6, 0x10);
	mdelay(1);

	/* ##LEQ Read Out */
	leq_gray = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 7, 3);
	leq_manual = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x18, 15, 15);
	leq_bin = rtpcs_930x_sds_rxcal_gray_to_binary(leq_gray);

	pr_info("LEQ_gray: %u, LEQ_bin: %u", leq_gray, leq_bin);
	pr_info("LEQ manual: %u", leq_manual);

	return leq_bin;
}

static void rtpcs_930x_sds_rxcal_vth_manual(struct rtpcs_ctrl *ctrl, u32 sds_num,
					    bool manual, u32 vth_list[])
{
	if (manual) {
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, 13, 13, 0x1);
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x13,  5,  3, vth_list[0]);
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x13,  2,  0, vth_list[1]);
	} else {
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, 13, 13, 0x0);
		mdelay(10);
	}
}

static void rtpcs_930x_sds_rxcal_vth_get(struct rtpcs_ctrl *ctrl, u32 sds_num,
					 u32 vth_list[])
{
	u32 vth_manual;

	/* ##Page0x1F, Reg0x02[15 0], REG_DBGO_SEL=[0x002F]; */ /* Lane0 */
	/* ##Page0x1F, Reg0x02[15 0], REG_DBGO_SEL=[0x0031]; */ /* Lane1 */
	if (!(sds_num % 2))
		rtpcs_sds_write(ctrl, sds_num, 0x1f, 0x2, 0x2f);
	else
		rtpcs_sds_write(ctrl, sds_num - 1, 0x1f, 0x2, 0x31);

	/* ##Page0x2E, Reg0x15[9], REG0_RX_EN_TEST=[1] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x15, 9, 9, 0x1);
	/* ##Page0x21, Reg0x06[11 6], REG0_RX_DEBUG_SEL=[1 0 x x x x] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x21, 0x06, 11, 6, 0x20);
	/* ##Page0x2F, Reg0x0C[5 0], REG0_COEF_SEL=[0 0 1 1 0 0] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0xc);

	mdelay(1);

	/* ##VthP & VthN Read Out */
	vth_list[0] = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 2, 0); /* v_thp set bin */
	vth_list[1] = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 5, 3); /* v_thn set bin */

	pr_info("vth_set_bin = %d", vth_list[0]);
	pr_info("vth_set_bin = %d", vth_list[1]);

	vth_manual = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x0f, 13, 13);
	pr_info("Vth Maunal = %d", vth_manual);
}

static void rtpcs_930x_sds_rxcal_tap_manual(struct rtpcs_ctrl *ctrl, u32 sds_num,
					    int tap_id, bool manual, u32 tap_list[])
{
	if (manual) {
		switch(tap_id) {
		case 0:
			/* ##REG0_LOAD_IN_INIT[0]=1; REG0_TAP0_INIT[5:0]=Tap0_Value */
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, tap_id + 7, tap_id + 7, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x03, 5, 5, tap_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x03, 4, 0, tap_list[1]);
			break;
		case 1:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, tap_id + 7, tap_id + 7, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x21, 0x07, 6, 6, tap_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x09, 11, 6, tap_list[1]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x21, 0x07, 5, 5, tap_list[2]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x12, 5, 0, tap_list[3]);
			break;
		case 2:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, tap_id + 7, tap_id + 7, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x09, 5, 5, tap_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x09, 4, 0, tap_list[1]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0a, 11, 11, tap_list[2]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0a, 10, 6, tap_list[3]);
			break;
		case 3:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, tap_id + 7, tap_id + 7, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0a, 5, 5, tap_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0a, 4, 0, tap_list[1]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x06, 5, 5, tap_list[2]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x06, 4, 0, tap_list[3]);
			break;
		case 4:
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, tap_id + 7, tap_id + 7, 0x1);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x01, 5, 5, tap_list[0]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x01, 4, 0, tap_list[1]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x06, 11, 11, tap_list[2]);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x06, 10, 6, tap_list[3]);
			break;
		default:
			break;
		}
	} else {
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x0f, tap_id + 7, tap_id + 7, 0x0);
		mdelay(10);
	}
}

static void rtpcs_930x_sds_rxcal_tap_get(struct rtpcs_ctrl *ctrl, u32 sds_num,
					 u32 tap_id, u32 tap_list[])
{
	u32 tap0_sign_out;
	u32 tap0_coef_bin;
	u32 tap_sign_out_even;
	u32 tap_coef_bin_even;
	u32 tap_sign_out_odd;
	u32 tap_coef_bin_odd;
	bool tap_manual;

	if (!(sds_num % 2))
		rtpcs_sds_write(ctrl, sds_num, 0x1f, 0x2, 0x2f);
	else
		rtpcs_sds_write(ctrl, sds_num - 1, 0x1f, 0x2, 0x31);

	/* ##Page0x2E, Reg0x15[9], REG0_RX_EN_TEST=[1] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x15, 9, 9, 0x1);
	/* ##Page0x21, Reg0x06[11 6], REG0_RX_DEBUG_SEL=[1 0 x x x x] */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x21, 0x06, 11, 6, 0x20);

	if (!tap_id) {
		/* ##Page0x2F, Reg0x0C[5 0], REG0_COEF_SEL=[0 0 0 0 0 1] */
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0);
		/* ##Tap1 Even Read Out */
		mdelay(1);
		tap0_sign_out = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 5, 5);
		tap0_coef_bin = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 4, 0);

		if (tap0_sign_out == 1)
			pr_info("Tap0 Sign : -");
		else
			pr_info("Tap0 Sign : +");

		pr_info("tap0_coef_bin = %d", tap0_coef_bin);

		tap_list[0] = tap0_sign_out;
		tap_list[1] = tap0_coef_bin;

		tap_manual = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x0f, 7, 7);
		pr_info("tap0 manual = %u",tap_manual);
	} else {
		/* ##Page0x2F, Reg0x0C[5 0], REG0_COEF_SEL=[0 0 0 0 0 1] */
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, tap_id);
		mdelay(1);
		/* ##Tap1 Even Read Out */
		tap_sign_out_even = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 5, 5);
		tap_coef_bin_even = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 4, 0);

		/* ##Page0x2F, Reg0x0C[5 0], REG0_COEF_SEL=[0 0 0 1 1 0] */
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, (tap_id + 5));
		/* ##Tap1 Odd Read Out */
		tap_sign_out_odd = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 5, 5);
		tap_coef_bin_odd = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 4, 0);

		if (tap_sign_out_even == 1)
			pr_info("Tap %u even sign: -", tap_id);
		else
			pr_info("Tap %u even sign: +", tap_id);

		pr_info("Tap %u even coefficient = %u", tap_id, tap_coef_bin_even);

		if (tap_sign_out_odd == 1)
			pr_info("Tap %u odd sign: -", tap_id);
		else
			pr_info("Tap %u odd sign: +", tap_id);

		pr_info("Tap %u odd coefficient = %u", tap_id,tap_coef_bin_odd);

		tap_list[0] = tap_sign_out_even;
		tap_list[1] = tap_coef_bin_even;
		tap_list[2] = tap_sign_out_odd;
		tap_list[3] = tap_coef_bin_odd;

		tap_manual = rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x0f, tap_id + 7, tap_id + 7);
		pr_info("tap %u manual = %d",tap_id, tap_manual);
	}
}

__attribute__((unused))
static void rtpcs_930x_sds_do_rx_calibration_1(struct rtpcs_ctrl *ctrl, int sds,
					       phy_interface_t phy_mode)
{
	/* From both rtl9300_rxCaliConf_serdes_myParam and rtl9300_rxCaliConf_phy_myParam */
	int tap0_init_val = 0x1f; /* Initial Decision Fed Equalizer 0 tap */
	int vth_min       = 0x0;

	pr_info("start_1.1.1 initial value for sds %d\n", sds);
	rtpcs_sds_write(ctrl, sds, 6,  0, 0);

	/* FGCAL */
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x01, 14, 14, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x1c, 10,  5, 0x20);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x02,  0,  0, 0x01);

	/* DCVS */
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x1e, 14, 11, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x01, 15, 15, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x02, 11, 11, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x1c,  4,  0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x1d, 15, 11, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x1d, 10,  6, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x1d,  5,  1, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x02, 10,  6, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x11,  4,  0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x00,  3,  0, 0x0f);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x04,  6,  6, 0x01);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x04,  7,  7, 0x01);

	/* LEQ (Long Term Equivalent signal level) */
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x16, 14,  8, 0x00);

	/* DFE (Decision Fed Equalizer) */
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x03,  5,  0, tap0_init_val);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x09, 11,  6, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x09,  5,  0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0a,  5,  0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x01,  5,  0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x12,  5,  0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0a, 11,  6, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x06,  5,  0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x01,  5,  0, 0x00);

	/* Vth */
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x13,  5,  3, 0x07);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x13,  2,  0, 0x07);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x0b,  5,  3, vth_min);

	pr_info("end_1.1.1 --\n");

	pr_info("start_1.1.2 Load DFE init. value\n");

	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0f, 13,  7, 0x7f);

	pr_info("end_1.1.2\n");

	pr_info("start_1.1.3 disable LEQ training,enable DFE clock\n");

	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x17,  7,  7, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x17,  6,  2, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0c,  8,  8, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0b,  4,  4, 0x01);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x12, 14, 14, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x02, 15, 15, 0x00);

	pr_info("end_1.1.3 --\n");

	pr_info("start_1.1.4 offset cali setting\n");

	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0f, 15, 14, 0x03);

	pr_info("end_1.1.4\n");

	pr_info("start_1.1.5 LEQ and DFE setting\n");

	/* TODO: make this work for DAC cables of different lengths */
	/* For a 10GBit serdes wit Fibre, SDS 8 or 9 */
	if (phy_mode == PHY_INTERFACE_MODE_10GBASER ||
	    phy_mode == PHY_INTERFACE_MODE_1000BASEX ||
	    phy_mode == PHY_INTERFACE_MODE_SGMII)
		rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x16,  3,  2, 0x02);
	else
		pr_err("%s not PHY-based or SerDes, implement DAC!\n", __func__);

	/* No serdes, check for Aquantia PHYs */
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x16,  3,  2, 0x02);

	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0f,  6,  0, 0x5f);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x05,  7,  2, 0x1f);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x19,  9,  5, 0x1f);
	rtpcs_sds_write_bits(ctrl, sds, 0x2f, 0x0b, 15,  9, 0x3c);
	rtpcs_sds_write_bits(ctrl, sds, 0x2e, 0x0b,  1,  0, 0x03);

	pr_info("end_1.1.5\n");
}

static void rtpcs_930x_sds_do_rx_calibration_2_1(struct rtpcs_ctrl *ctrl,
						 u32 sds_num)
{
	pr_info("start_1.2.1 ForegroundOffsetCal_Manual\n");

	/* Gray config endis to 1 */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x02,  2,  2, 0x01);

	/* ForegroundOffsetCal_Manual(auto mode) */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x01, 14, 14, 0x00);

	pr_info("end_1.2.1");
}

static void rtpcs_930x_sds_do_rx_calibration_2_2(struct rtpcs_ctrl *ctrl,
						 int sds_num)
{
	/* Force Rx-Run = 0 */
	rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x15, 8, 8, 0x0);

	rtpcs_930x_sds_rx_reset(ctrl, sds_num, PHY_INTERFACE_MODE_10GBASER);
}

static void rtpcs_930x_sds_do_rx_calibration_2_3(struct rtpcs_ctrl *ctrl,
						 int sds_num)
{
	u32 fgcal_binary, fgcal_gray;
	u32 offset_range;

	pr_info("start_1.2.3 Foreground Calibration\n");

	while(1) {
		if (!(sds_num % 2))
			rtpcs_sds_write(ctrl, sds_num, 0x1f, 0x2, 0x2f);
		else
			rtpcs_sds_write(ctrl, sds_num - 1, 0x1f, 0x2, 0x31);

		/* ##Page0x2E, Reg0x15[9], REG0_RX_EN_TEST=[1] */
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x15, 9, 9, 0x1);
		/* ##Page0x21, Reg0x06[11 6], REG0_RX_DEBUG_SEL=[1 0 x x x x] */
		rtpcs_sds_write_bits(ctrl, sds_num, 0x21, 0x06, 11, 6, 0x20);
		/* ##Page0x2F, Reg0x0C[5 0], REG0_COEF_SEL=[0 0 1 1 1 1] */
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0xf);
		/* ##FGCAL read gray */
		fgcal_gray = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 5, 0);
		/* ##Page0x2F, Reg0x0C[5 0], REG0_COEF_SEL=[0 0 1 1 1 0] */
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2f, 0x0c, 5, 0, 0xe);
		/* ##FGCAL read binary */
		fgcal_binary = rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 5, 0);

		pr_info("%s: fgcal_gray: %d, fgcal_binary %d\n",
		        __func__, fgcal_gray, fgcal_binary);

		offset_range = rtpcs_sds_read_bits(ctrl, sds_num, 0x2e, 0x15, 15, 14);

		if (fgcal_binary > 60 || fgcal_binary < 3) {
			if (offset_range == 3) {
				pr_info("%s: Foreground Calibration result marginal!", __func__);
				break;
			} else {
				offset_range++;
				rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x15, 15, 14, offset_range);
				rtpcs_930x_sds_do_rx_calibration_2_2(ctrl, sds_num);
			}
		} else {
			break;
		}
	}
	pr_info("%s: end_1.2.3\n", __func__);
}

__attribute__((unused))
static void rtpcs_930x_sds_do_rx_calibration_2(struct rtpcs_ctrl *ctrl, int sds)
{
	rtpcs_930x_sds_rx_reset(ctrl, sds, PHY_INTERFACE_MODE_10GBASER);
	rtpcs_930x_sds_do_rx_calibration_2_1(ctrl, sds);
	rtpcs_930x_sds_do_rx_calibration_2_2(ctrl, sds);
	rtpcs_930x_sds_do_rx_calibration_2_3(ctrl, sds);
}

static void rtpcs_930x_sds_rxcal_3_1(struct rtpcs_ctrl *ctrl, int sds_num,
				     phy_interface_t phy_mode)
{
	pr_info("start_1.3.1");

	/* ##1.3.1 */
	if (phy_mode != PHY_INTERFACE_MODE_10GBASER &&
	    phy_mode != PHY_INTERFACE_MODE_1000BASEX &&
	    phy_mode != PHY_INTERFACE_MODE_SGMII)
		rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0xc, 8, 8, 0);

	rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x17, 7, 7, 0x0);
	rtpcs_930x_sds_rxcal_leq_manual(ctrl, sds_num, false, 0);

	pr_info("end_1.3.1");
}

static void rtpcs_930x_sds_rxcal_3_2(struct rtpcs_ctrl *ctrl, int sds_num,
				     phy_interface_t phy_mode)
{
	u32 sum10 = 0, avg10, int10;
	int dac_long_cable_offset;
	bool eq_hold_enabled;
	int i;

	if (phy_mode == PHY_INTERFACE_MODE_10GBASER ||
	    phy_mode == PHY_INTERFACE_MODE_1000BASEX ||
	    phy_mode == PHY_INTERFACE_MODE_SGMII) {
		/* rtl9300_rxCaliConf_serdes_myParam */
		dac_long_cable_offset = 3;
		eq_hold_enabled = true;
	} else {
		/* rtl9300_rxCaliConf_phy_myParam */
		dac_long_cable_offset = 0;
		eq_hold_enabled = false;
	}

	if (phy_mode != PHY_INTERFACE_MODE_10GBASER)
		pr_warn("%s: LEQ only valid for 10GR!\n", __func__);

	pr_info("start_1.3.2");

	for(i = 0; i < 10; i++) {
		sum10 += rtpcs_930x_sds_rxcal_leq_read(ctrl, sds_num);
		mdelay(10);
	}

	avg10 = (sum10 / 10) + (((sum10 % 10) >= 5) ? 1 : 0);
	int10 = sum10 / 10;

	pr_info("sum10:%u, avg10:%u, int10:%u", sum10, avg10, int10);

	if (phy_mode == PHY_INTERFACE_MODE_10GBASER ||
	    phy_mode == PHY_INTERFACE_MODE_1000BASEX ||
	    phy_mode == PHY_INTERFACE_MODE_SGMII) {
		if (dac_long_cable_offset) {
			rtpcs_930x_sds_rxcal_leq_offset_manual(ctrl, sds_num, 1,
							       dac_long_cable_offset);
			rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x17, 7, 7,
					     eq_hold_enabled);
			if (phy_mode == PHY_INTERFACE_MODE_10GBASER)
				rtpcs_930x_sds_rxcal_leq_manual(ctrl, sds_num,
								true, avg10);
		} else {
			if (sum10 >= 5) {
				rtpcs_930x_sds_rxcal_leq_offset_manual(ctrl, sds_num, 1, 3);
				rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x17, 7, 7, 0x1);
				if (phy_mode == PHY_INTERFACE_MODE_10GBASER)
					rtpcs_930x_sds_rxcal_leq_manual(ctrl, sds_num, true, avg10);
			} else {
				rtpcs_930x_sds_rxcal_leq_offset_manual(ctrl, sds_num, 1, 0);
				rtpcs_sds_write_bits(ctrl, sds_num, 0x2e, 0x17, 7, 7, 0x1);
				if (phy_mode == PHY_INTERFACE_MODE_10GBASER)
					rtpcs_930x_sds_rxcal_leq_manual(ctrl, sds_num, true, avg10);
			}
		}
	}

	pr_info("Sds:%u LEQ = %u",sds_num, rtpcs_930x_sds_rxcal_leq_read(ctrl, sds_num));

	pr_info("end_1.3.2");
}

__attribute__((unused))
static void rtpcs_930x_sds_do_rx_calibration_3(struct rtpcs_ctrl *ctrl, int sds_num,
					       phy_interface_t phy_mode)
{
	rtpcs_930x_sds_rxcal_3_1(ctrl, sds_num, phy_mode);

	if (phy_mode == PHY_INTERFACE_MODE_10GBASER ||
	    phy_mode == PHY_INTERFACE_MODE_1000BASEX ||
	    phy_mode == PHY_INTERFACE_MODE_SGMII)
		rtpcs_930x_sds_rxcal_3_2(ctrl, sds_num, phy_mode);
}

static void rtpcs_930x_sds_do_rx_calibration_4_1(struct rtpcs_ctrl *ctrl, int sds_num)
{
	u32 vth_list[2] = {0, 0};
	u32 tap0_list[4] = {0, 0, 0, 0};

	pr_info("start_1.4.1");

	/* ##1.4.1 */
	rtpcs_930x_sds_rxcal_vth_manual(ctrl, sds_num, false, vth_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 0, false, tap0_list);
	mdelay(200);

	pr_info("end_1.4.1");
}

static void rtpcs_930x_sds_do_rx_calibration_4_2(struct rtpcs_ctrl *ctrl, u32 sds_num)
{
	u32 vth_list[2];
	u32 tap_list[4];

	pr_info("start_1.4.2");

	rtpcs_930x_sds_rxcal_vth_get(ctrl, sds_num, vth_list);
	rtpcs_930x_sds_rxcal_vth_manual(ctrl, sds_num, true, vth_list);

	mdelay(100);

	rtpcs_930x_sds_rxcal_tap_get(ctrl, sds_num, 0, tap_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 0, true, tap_list);

	pr_info("end_1.4.2");
}

static void rtpcs_930x_sds_do_rx_calibration_4(struct rtpcs_ctrl *ctrl, u32 sds_num)
{
	rtpcs_930x_sds_do_rx_calibration_4_1(ctrl, sds_num);
	rtpcs_930x_sds_do_rx_calibration_4_2(ctrl, sds_num);
}

static void rtpcs_930x_sds_do_rx_calibration_5_2(struct rtpcs_ctrl *ctrl, u32 sds_num)
{
	u32 tap1_list[4] = {0};
	u32 tap2_list[4] = {0};
	u32 tap3_list[4] = {0};
	u32 tap4_list[4] = {0};

	pr_info("start_1.5.2");

	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 1, false, tap1_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 2, false, tap2_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 3, false, tap3_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 4, false, tap4_list);

	mdelay(30);

	pr_info("end_1.5.2");
}

static void rtpcs_930x_sds_do_rx_calibration_5(struct rtpcs_ctrl *ctrl, u32 sds_num,
					       phy_interface_t phy_mode)
{
	if (phy_mode == PHY_INTERFACE_MODE_10GBASER) /* dfeTap1_4Enable true */
		rtpcs_930x_sds_do_rx_calibration_5_2(ctrl, sds_num);
}


static void rtpcs_930x_sds_do_rx_calibration_dfe_disable(struct rtpcs_ctrl *ctrl, u32 sds_num)
{
	u32 tap1_list[4] = {0};
	u32 tap2_list[4] = {0};
	u32 tap3_list[4] = {0};
	u32 tap4_list[4] = {0};

	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 1, true, tap1_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 2, true, tap2_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 3, true, tap3_list);
	rtpcs_930x_sds_rxcal_tap_manual(ctrl, sds_num, 4, true, tap4_list);

	mdelay(10);
}

static void rtpcs_930x_sds_do_rx_calibration(struct rtpcs_ctrl *ctrl, int sds,
					     phy_interface_t phy_mode)
{
	u32 latch_sts;

	rtpcs_930x_sds_do_rx_calibration_1(ctrl, sds, phy_mode);
	rtpcs_930x_sds_do_rx_calibration_2(ctrl, sds);
	rtpcs_930x_sds_do_rx_calibration_4(ctrl, sds);
	rtpcs_930x_sds_do_rx_calibration_5(ctrl, sds, phy_mode);
	mdelay(20);

	/* Do this only for 10GR mode, SDS active in mode 0x1a */
	if (rtpcs_sds_read_bits(ctrl, sds, 0x1f, 9, 11, 7) == RTL930X_SDS_MODE_10GBASER) {
		pr_info("%s: SDS enabled\n", __func__);
		latch_sts = rtpcs_sds_read_bits(ctrl, sds, 0x4, 1, 2, 2);
		mdelay(1);
		latch_sts = rtpcs_sds_read_bits(ctrl, sds, 0x4, 1, 2, 2);
		if (latch_sts) {
			rtpcs_930x_sds_do_rx_calibration_dfe_disable(ctrl, sds);
			rtpcs_930x_sds_do_rx_calibration_4(ctrl, sds);
			rtpcs_930x_sds_do_rx_calibration_5(ctrl, sds, phy_mode);
		}
	}
}

static int rtpcs_930x_sds_sym_err_reset(struct rtpcs_ctrl *ctrl, int sds_num,
					phy_interface_t phy_mode)
{
	switch (phy_mode) {
	case PHY_INTERFACE_MODE_XGMII:
		break;

	case PHY_INTERFACE_MODE_10GBASER:
		/* Read twice to clear */
		rtpcs_sds_read(ctrl, sds_num, 5, 1);
		rtpcs_sds_read(ctrl, sds_num, 5, 1);
		break;

	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_10G_QXGMII:
		rtpcs_sds_write_bits(ctrl, sds_num, 0x1, 24, 2, 0, 0);
		rtpcs_sds_write_bits(ctrl, sds_num, 0x1, 3, 15, 8, 0);
		rtpcs_sds_write_bits(ctrl, sds_num, 0x1, 2, 15, 0, 0);
		break;

	default:
		pr_info("%s unsupported phy mode\n", __func__);
		return -1;
	}

	return 0;
}

static u32 rtpcs_930x_sds_sym_err_get(struct rtpcs_ctrl *ctrl, int sds_num,
				      phy_interface_t phy_mode)
{
	u32 v = 0;

	switch (phy_mode) {
	case PHY_INTERFACE_MODE_XGMII:
	case PHY_INTERFACE_MODE_10G_QXGMII:
		break;

	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_10GBASER:
		v = rtpcs_sds_read(ctrl, sds_num, 5, 1);
		return v & 0xff;

	default:
		pr_info("%s unsupported PHY-mode\n", __func__);
	}

	return v;
}

static int rtpcs_930x_sds_check_calibration(struct rtpcs_ctrl *ctrl, int sds_num,
					    phy_interface_t phy_mode)
{
	u32 errors1, errors2;

	rtpcs_930x_sds_sym_err_reset(ctrl, sds_num, phy_mode);
	rtpcs_930x_sds_sym_err_reset(ctrl, sds_num, phy_mode);

	/* Count errors during 1ms */
	errors1 = rtpcs_930x_sds_sym_err_get(ctrl, sds_num, phy_mode);
	mdelay(1);
	errors2 = rtpcs_930x_sds_sym_err_get(ctrl, sds_num, phy_mode);

	switch (phy_mode) {
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_XGMII:
		if ((errors2 - errors1 > 100) ||
		    (errors1 >= 0xffff00) || (errors2 >= 0xffff00)) {
			pr_info("%s XSGMII error rate too high\n", __func__);
			return 1;
		}
		break;
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_10G_QXGMII:
		if (errors2 > 0) {
			pr_info("%s: %s error rate too high\n", __func__, phy_modes(phy_mode));
			return 1;
		}
		break;
	default:
		return 1;
	}

	return 0;
}

static void rtpcs_930x_phy_enable_10g_1g(struct rtpcs_ctrl *ctrl, int sds_num)
{
	u32 v;

	/* Enable 1GBit PHY */
	v = rtpcs_sds_read(ctrl, sds_num, PHY_PAGE_2, MII_BMCR);
	pr_info("%s 1gbit phy: %08x\n", __func__, v);
	v &= ~BMCR_PDOWN;
	rtpcs_sds_write(ctrl, sds_num, PHY_PAGE_2, MII_BMCR, v);
	pr_info("%s 1gbit phy enabled: %08x\n", __func__, v);

	/* Enable 10GBit PHY */
	v = rtpcs_sds_read(ctrl, sds_num, PHY_PAGE_4, MII_BMCR);
	pr_info("%s 10gbit phy: %08x\n", __func__, v);
	v &= ~BMCR_PDOWN;
	rtpcs_sds_write(ctrl, sds_num, PHY_PAGE_4, MII_BMCR, v);
	pr_info("%s 10gbit phy after: %08x\n", __func__, v);

	/* dal_longan_construct_mac_default_10gmedia_fiber */
	v = rtpcs_sds_read(ctrl, sds_num, 0x1f, 11);
	pr_info("%s set medium: %08x\n", __func__, v);
	v |= BIT(1);
	rtpcs_sds_write(ctrl, sds_num, 0x1f, 11, v);
	pr_info("%s set medium after: %08x\n", __func__, v);
}

static int rtpcs_930x_sds_10g_idle(struct rtpcs_ctrl *ctrl, int sds_num)
{
	bool busy;
	int i = 0;

	do {
		if (sds_num % 2) {
			rtpcs_sds_write_bits(ctrl, sds_num - 1, 0x1f, 0x2, 15, 0, 53);
			busy = !!rtpcs_sds_read_bits(ctrl, sds_num - 1, 0x1f, 0x14, 1, 1);
		} else {
			rtpcs_sds_write_bits(ctrl, sds_num, 0x1f, 0x2, 15, 0, 53);
			busy = !!rtpcs_sds_read_bits(ctrl, sds_num, 0x1f, 0x14, 0, 0);
		}
		i++;
	} while (busy && i < 100);

	if (i < 100)
		return 0;

	pr_warn("%s WARNING: Waiting for RX idle timed out, SDS %d\n", __func__, sds_num);
	return -EIO;
}

static int rtpcs_930x_sds_set_polarity(struct rtpcs_ctrl *ctrl, u32 sds,
				       bool tx_inv, bool rx_inv)
{
	u8 rx_val = rx_inv ? 1 : 0;
	u8 tx_val = tx_inv ? 1 : 0;
	u32 val;
	int ret;

	/* 10GR */
	val = (tx_val << 1) | rx_val;
	ret = rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x2, 14, 13, val);
	if (ret)
		return ret;

	/* 1G */
	val = (rx_val << 1) | tx_val;
	return rtpcs_sds_write_bits(ctrl, sds, 0x0, 0x0, 9, 8, val);
}

static const sds_config rtpcs_930x_sds_cfg_10gr_even[] =
{
	/* 1G */
	{0x00, 0x0E, 0x3053}, {0x01, 0x14, 0x0100}, {0x21, 0x03, 0x8206},
	{0x21, 0x05, 0x40B0}, {0x21, 0x06, 0x0010}, {0x21, 0x07, 0xF09F},
	{0x21, 0x0C, 0x0007}, {0x21, 0x0D, 0x6009}, {0x21, 0x0E, 0x0000},
	{0x21, 0x0F, 0x0008}, {0x24, 0x00, 0x0668}, {0x24, 0x02, 0xD020},
	{0x24, 0x06, 0xC000}, {0x24, 0x0B, 0x1892}, {0x24, 0x0F, 0xFFDF},
	{0x24, 0x12, 0x03C4}, {0x24, 0x13, 0x027F}, {0x24, 0x14, 0x1311},
	{0x24, 0x16, 0x00C9}, {0x24, 0x17, 0xA100}, {0x24, 0x1A, 0x0001},
	{0x24, 0x1C, 0x0400}, {0x25, 0x01, 0x0300}, {0x25, 0x02, 0x1017},
	{0x25, 0x03, 0xFFDF}, {0x25, 0x05, 0x7F7C}, {0x25, 0x07, 0x8100},
	{0x25, 0x08, 0x0001}, {0x25, 0x09, 0xFFD4}, {0x25, 0x0A, 0x7C2F},
	{0x25, 0x0E, 0x003F}, {0x25, 0x0F, 0x0121}, {0x25, 0x10, 0x0020},
	{0x25, 0x11, 0x8840}, {0x2B, 0x13, 0x0050}, {0x2B, 0x18, 0x8E88},
	{0x2B, 0x19, 0x4902}, {0x2B, 0x1D, 0x2501}, {0x2D, 0x13, 0x0050},
	{0x2D, 0x18, 0x8E88}, {0x2D, 0x19, 0x4902}, {0x2D, 0x1D, 0x2641},
	{0x2F, 0x13, 0x0050}, {0x2F, 0x18, 0x8E88}, {0x2F, 0x19, 0x4902},
	{0x2F, 0x1D, 0x66E1},
	/* 3.125G */
	{0x28, 0x00, 0x0668}, {0x28, 0x02, 0xD020}, {0x28, 0x06, 0xC000},
	{0x28, 0x0B, 0x1892}, {0x28, 0x0F, 0xFFDF}, {0x28, 0x12, 0x01C4},
	{0x28, 0x13, 0x027F}, {0x28, 0x14, 0x1311}, {0x28, 0x16, 0x00C9},
	{0x28, 0x17, 0xA100}, {0x28, 0x1A, 0x0001}, {0x28, 0x1C, 0x0400},
	{0x29, 0x01, 0x0300}, {0x29, 0x02, 0x1017}, {0x29, 0x03, 0xFFDF},
	{0x29, 0x05, 0x7F7C}, {0x29, 0x07, 0x8100}, {0x29, 0x08, 0x0001},
	{0x29, 0x09, 0xFFD4}, {0x29, 0x0A, 0x7C2F}, {0x29, 0x0E, 0x003F},
	{0x29, 0x0F, 0x0121}, {0x29, 0x10, 0x0020}, {0x29, 0x11, 0x8840},
	/* 10G */
	{0x06, 0x0D, 0x0F00}, {0x06, 0x00, 0x0000}, {0x06, 0x01, 0xC800},
	{0x21, 0x03, 0x8206}, {0x21, 0x05, 0x40B0}, {0x21, 0x06, 0x0010},
	{0x21, 0x07, 0xF09F}, {0x21, 0x0C, 0x0007}, {0x21, 0x0D, 0x6009},
	{0x21, 0x0E, 0x0000}, {0x21, 0x0F, 0x0008}, {0x2E, 0x00, 0xA668},
	{0x2E, 0x02, 0xD020}, {0x2E, 0x06, 0xC000}, {0x2E, 0x0B, 0x1892},
	{0x2E, 0x0F, 0xFFDF}, {0x2E, 0x11, 0x8280}, {0x2E, 0x12, 0x0044},
	{0x2E, 0x13, 0x027F}, {0x2E, 0x14, 0x1311}, {0x2E, 0x17, 0xA100},
	{0x2E, 0x1A, 0x0001}, {0x2E, 0x1C, 0x0400}, {0x2F, 0x01, 0x0300},
	{0x2F, 0x02, 0x1217}, {0x2F, 0x03, 0xFFDF}, {0x2F, 0x05, 0x7F7C},
	{0x2F, 0x07, 0x80C4}, {0x2F, 0x08, 0x0001}, {0x2F, 0x09, 0xFFD4},
	{0x2F, 0x0A, 0x7C2F}, {0x2F, 0x0E, 0x003F}, {0x2F, 0x0F, 0x0121},
	{0x2F, 0x10, 0x0020}, {0x2F, 0x11, 0x8840}, {0x2F, 0x14, 0xE008},
	{0x2B, 0x13, 0x0050}, {0x2B, 0x18, 0x8E88}, {0x2B, 0x19, 0x4902},
	{0x2B, 0x1D, 0x2501}, {0x2D, 0x13, 0x0050}, {0x2D, 0x17, 0x4109},
	{0x2D, 0x18, 0x8E88}, {0x2D, 0x19, 0x4902}, {0x2D, 0x1C, 0x1109},
	{0x2D, 0x1D, 0x2641}, {0x2F, 0x13, 0x0050}, {0x2F, 0x18, 0x8E88},
	{0x2F, 0x19, 0x4902}, {0x2F, 0x1D, 0x76E1},
};

static const sds_config rtpcs_930x_sds_cfg_10gr_odd[] =
{
	/* 1G */
	{0x00, 0x0E, 0x3053}, {0x01, 0x14, 0x0100}, {0x21, 0x03, 0x8206},
	{0x21, 0x06, 0x0010}, {0x21, 0x07, 0xF09F}, {0x21, 0x0A, 0x0003},
	{0x21, 0x0B, 0x0005}, {0x21, 0x0C, 0x0007}, {0x21, 0x0D, 0x6009},
	{0x21, 0x0E, 0x0000}, {0x21, 0x0F, 0x0008}, {0x24, 0x00, 0x0668},
	{0x24, 0x02, 0xD020}, {0x24, 0x06, 0xC000}, {0x24, 0x0B, 0x1892},
	{0x24, 0x0F, 0xFFDF}, {0x24, 0x12, 0x03C4}, {0x24, 0x13, 0x027F},
	{0x24, 0x14, 0x1311}, {0x24, 0x16, 0x00C9}, {0x24, 0x17, 0xA100},
	{0x24, 0x1A, 0x0001}, {0x24, 0x1C, 0x0400}, {0x25, 0x00, 0x820F},
	{0x25, 0x01, 0x0300}, {0x25, 0x02, 0x1017}, {0x25, 0x03, 0xFFDF},
	{0x25, 0x05, 0x7F7C}, {0x25, 0x07, 0x8100}, {0x25, 0x08, 0x0001},
	{0x25, 0x09, 0xFFD4}, {0x25, 0x0A, 0x7C2F}, {0x25, 0x0E, 0x003F},
	{0x25, 0x0F, 0x0121}, {0x25, 0x10, 0x0020}, {0x25, 0x11, 0x8840},
	{0x2B, 0x13, 0x3D87}, {0x2B, 0x14, 0x3108}, {0x2D, 0x13, 0x3C87},
	{0x2D, 0x14, 0x1808},
	/* 3.125G */
	{0x28, 0x00, 0x0668}, {0x28, 0x02, 0xD020}, {0x28, 0x06, 0xC000},
	{0x28, 0x0B, 0x1892}, {0x28, 0x0F, 0xFFDF}, {0x28, 0x12, 0x01C4},
	{0x28, 0x13, 0x027F}, {0x28, 0x14, 0x1311}, {0x28, 0x16, 0x00C9},
	{0x28, 0x17, 0xA100}, {0x28, 0x1A, 0x0001}, {0x28, 0x1C, 0x0400},
	{0x29, 0x00, 0x820F}, {0x29, 0x01, 0x0300}, {0x29, 0x02, 0x1017},
	{0x29, 0x03, 0xFFDF}, {0x29, 0x05, 0x7F7C}, {0x29, 0x07, 0x8100},
	{0x29, 0x08, 0x0001}, {0x29, 0x0A, 0x7C2F}, {0x29, 0x0E, 0x003F},
	{0x29, 0x0F, 0x0121}, {0x29, 0x10, 0x0020}, {0x29, 0x11, 0x8840},
	/* 10G */
	{0x06, 0x0D, 0x0F00}, {0x06, 0x00, 0x0000}, {0x06, 0x01, 0xC800},
	{0x21, 0x03, 0x8206}, {0x21, 0x05, 0x40B0}, {0x21, 0x06, 0x0010},
	{0x21, 0x07, 0xF09F}, {0x21, 0x0A, 0x0003}, {0x21, 0x0B, 0x0005},
	{0x21, 0x0C, 0x0007}, {0x21, 0x0D, 0x6009}, {0x21, 0x0E, 0x0000},
	{0x21, 0x0F, 0x0008}, {0x2E, 0x00, 0xA668}, {0x2E, 0x02, 0xD020},
	{0x2E, 0x06, 0xC000}, {0x2E, 0x0B, 0x1892}, {0x2E, 0x0F, 0xFFDF},
	{0x2E, 0x11, 0x8280}, {0x2E, 0x12, 0x0044}, {0x2E, 0x13, 0x027F},
	{0x2E, 0x14, 0x1311}, {0x2E, 0x17, 0xA100}, {0x2E, 0x1A, 0x0001},
	{0x2E, 0x1C, 0x0400}, {0x2F, 0x00, 0x820F}, {0x2F, 0x01, 0x0300},
	{0x2F, 0x02, 0x1217}, {0x2F, 0x03, 0xFFDF}, {0x2F, 0x05, 0x7F7C},
	{0x2F, 0x07, 0x80C4}, {0x2F, 0x08, 0x0001}, {0x2F, 0x09, 0xFFD4},
	{0x2F, 0x0A, 0x7C2F}, {0x2F, 0x0E, 0x003F}, {0x2F, 0x0F, 0x0121},
	{0x2F, 0x10, 0x0020}, {0x2F, 0x11, 0x8840}, {0x2B, 0x13, 0x3D87},
	{0x2B, 0x14, 0x3108}, {0x2D, 0x13, 0x3C87}, {0x2D, 0x14, 0x1808},
};

static const sds_config rtpcs_930x_sds_cfg_10g_2500bx_even[] =
{
	{0x00, 0x0E, 0x3053}, {0x01, 0x14, 0x0100},
	{0x21, 0x03, 0x8206}, {0x21, 0x05, 0x40B0}, {0x21, 0x06, 0x0010}, {0x21, 0x07, 0xF09F},
	{0x21, 0x0C, 0x0007}, {0x21, 0x0D, 0x6009}, {0x21, 0x0E, 0x0000}, {0x21, 0x0F, 0x0008},
	{0x24, 0x00, 0x0668}, {0x24, 0x02, 0xD020}, {0x24, 0x06, 0xC000}, {0x24, 0x0B, 0x1892},
	{0x24, 0x0F, 0xFFDF}, {0x24, 0x12, 0x03C4}, {0x24, 0x13, 0x027F}, {0x24, 0x14, 0x1311},
	{0x24, 0x16, 0x00C9}, {0x24, 0x17, 0xA100}, {0x24, 0x1A, 0x0001}, {0x24, 0x1C, 0x0400},
	{0x25, 0x01, 0x0300}, {0x25, 0x02, 0x1017}, {0x25, 0x03, 0xFFDF}, {0x25, 0x05, 0x7F7C},
	{0x25, 0x07, 0x8100}, {0x25, 0x08, 0x0001}, {0x25, 0x09, 0xFFD4}, {0x25, 0x0A, 0x7C2F},
	{0x25, 0x0E, 0x003F}, {0x25, 0x0F, 0x0121}, {0x25, 0x10, 0x0020}, {0x25, 0x11, 0x8840},
	{0x28, 0x00, 0x0668}, {0x28, 0x02, 0xD020}, {0x28, 0x06, 0xC000}, {0x28, 0x0B, 0x1892},
	{0x28, 0x0F, 0xFFDF}, {0x28, 0x12, 0x01C4}, {0x28, 0x13, 0x027F}, {0x28, 0x14, 0x1311},
	{0x28, 0x16, 0x00C9}, {0x28, 0x17, 0xA100}, {0x28, 0x1A, 0x0001}, {0x28, 0x1C, 0x0400},
	{0x29, 0x01, 0x0300}, {0x29, 0x02, 0x1017}, {0x29, 0x03, 0xFFDF}, {0x29, 0x05, 0x7F7C},
	{0x29, 0x07, 0x8100}, {0x29, 0x08, 0x0001}, {0x29, 0x09, 0xFFD4}, {0x29, 0x0A, 0x7C2F},
	{0x29, 0x0E, 0x003F}, {0x29, 0x0F, 0x0121}, {0x29, 0x10, 0x0020}, {0x29, 0x11, 0x8840},
	{0x2B, 0x13, 0x0050}, {0x2B, 0x18, 0x8E88}, {0x2B, 0x19, 0x4902}, {0x2B, 0x1D, 0x2501},
	{0x2D, 0x13, 0x0050}, {0x2D, 0x18, 0x8E88}, {0x2D, 0x17, 0x4109}, {0x2D, 0x19, 0x4902},
	{0x2D, 0x1C, 0x1109}, {0x2D, 0x1D, 0x2641},
	{0x2F, 0x13, 0x0050}, {0x2F, 0x18, 0x8E88}, {0x2F, 0x19, 0x4902}, {0x2F, 0x1D, 0x66E1},
};

static const sds_config rtpcs_930x_sds_cfg_10g_2500bx_odd[] =
{
	{0x00, 0x0E, 0x3053}, {0x01, 0x14, 0x0100},
	{0x21, 0x03, 0x8206}, {0x21, 0x06, 0x0010}, {0x21, 0x07, 0xF09F}, {0x21, 0x0A, 0x0003},
	{0x21, 0x0B, 0x0005}, {0x21, 0x0C, 0x0007}, {0x21, 0x0D, 0x6009}, {0x21, 0x0E, 0x0000},
	{0x21, 0x0F, 0x0008},
	{0x24, 0x00, 0x0668}, {0x24, 0x02, 0xD020}, {0x24, 0x06, 0xC000}, {0x24, 0x0B, 0x1892},
	{0x24, 0x0F, 0xFFDF}, {0x24, 0x12, 0x03C4}, {0x24, 0x13, 0x027F}, {0x24, 0x14, 0x1311},
	{0x24, 0x16, 0x00C9}, {0x24, 0x17, 0xA100}, {0x24, 0x1A, 0x0001}, {0x24, 0x1C, 0x0400},
	{0x25, 0x00, 0x820F}, {0x25, 0x01, 0x0300}, {0x25, 0x02, 0x1017}, {0x25, 0x03, 0xFFDF},
	{0x25, 0x05, 0x7F7C}, {0x25, 0x07, 0x8100}, {0x25, 0x08, 0x0001}, {0x25, 0x09, 0xFFD4},
	{0x25, 0x0A, 0x7C2F}, {0x25, 0x0E, 0x003F}, {0x25, 0x0F, 0x0121}, {0x25, 0x10, 0x0020},
	{0x25, 0x11, 0x8840},
	{0x28, 0x00, 0x0668}, {0x28, 0x02, 0xD020}, {0x28, 0x06, 0xC000}, {0x28, 0x0B, 0x1892},
	{0x28, 0x0F, 0xFFDF}, {0x28, 0x12, 0x01C4}, {0x28, 0x13, 0x027F}, {0x28, 0x14, 0x1311},
	{0x28, 0x16, 0x00C9}, {0x28, 0x17, 0xA100}, {0x28, 0x1A, 0x0001}, {0x28, 0x1C, 0x0400},
	{0x29, 0x00, 0x820F}, {0x29, 0x01, 0x0300}, {0x29, 0x02, 0x1017}, {0x29, 0x03, 0xFFDF},
	{0x29, 0x05, 0x7F7C}, {0x29, 0x07, 0x8100}, {0x29, 0x08, 0x0001}, {0x29, 0x0A, 0x7C2F},
	{0x29, 0x0E, 0x003F}, {0x29, 0x0F, 0x0121}, {0x29, 0x10, 0x0020}, {0x29, 0x11, 0x8840},
	{0x2B, 0x13, 0x3D87}, {0x2B, 0x14, 0x3108},
	{0x2D, 0x13, 0x3C87}, {0x2D, 0x14, 0x1808},
};

static void rtpcs_930x_sds_usxgmii_config(struct rtpcs_ctrl *ctrl, int sds,
					  int nway_en, u32 opcode, u32 am_period,
					  u32 all_am_markers, u32 an_table,
					  u32 sync_bit)
{
	rtpcs_sds_write_bits(ctrl, sds, 0x7, 0x11, 0, 0, nway_en);
	rtpcs_sds_write_bits(ctrl, sds, 0x7, 0x11, 1, 1, nway_en);
	rtpcs_sds_write_bits(ctrl, sds, 0x7, 0x11, 2, 2, nway_en);
	rtpcs_sds_write_bits(ctrl, sds, 0x7, 0x11, 3, 3, nway_en);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x12, 15, 0, am_period);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x13, 7,  0, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x13, 15, 8, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x14, 7,  0, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x14, 15, 8, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x15, 7,  0, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x15, 15, 8, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x16, 7,  0, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x16, 15, 8, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x17, 7,  0, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x17, 15, 8, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x18, 7,  0, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x18, 15, 8, all_am_markers);
	rtpcs_sds_write_bits(ctrl, sds, 0x7, 0x10, 7, 0, opcode);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0xe, 10, 10, an_table);
	rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x1d, 11, 10, sync_bit);
}

static void rtpcs_930x_sds_patch(struct rtpcs_ctrl *ctrl, int sds, phy_interface_t mode)
{
	const bool even_sds = ((sds & 1) == 0);
	const sds_config *config;
	size_t count;

	switch (mode) {
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_10GBASER:
		if (even_sds) {
			config = rtpcs_930x_sds_cfg_10gr_even;
			count = ARRAY_SIZE(rtpcs_930x_sds_cfg_10gr_even);
		} else {
			config = rtpcs_930x_sds_cfg_10gr_odd;
			count = ARRAY_SIZE(rtpcs_930x_sds_cfg_10gr_odd);
		}
		break;

	case PHY_INTERFACE_MODE_2500BASEX:
		if (even_sds) {
			config = rtpcs_930x_sds_cfg_10g_2500bx_even;
			count = ARRAY_SIZE(rtpcs_930x_sds_cfg_10g_2500bx_even);
		} else {
			config = rtpcs_930x_sds_cfg_10g_2500bx_odd;
			count = ARRAY_SIZE(rtpcs_930x_sds_cfg_10g_2500bx_odd);
		}
		break;

	case PHY_INTERFACE_MODE_10G_QXGMII:
		return;

	default:
		pr_warn("%s: unsupported mode %s on serdes %d\n", __func__, phy_modes(mode), sds);
		return;
	}

	for (size_t i = 0; i < count; ++i) {
		rtpcs_sds_write(ctrl, sds, config[i].page, config[i].reg, config[i].data);
	}

	if (mode == PHY_INTERFACE_MODE_10G_QXGMII) {
		/* Default configuration */
		rtpcs_930x_sds_usxgmii_config(ctrl, sds, 1, 0xaa, 0x5078, 0, 1, 0x1);
	}
}

__attribute__((unused))
static int rtpcs_930x_sds_cmu_band_get(struct rtpcs_ctrl *ctrl, int sds)
{
	u32 page;
	u32 en;
	u32 cmu_band;

/*	page = rtl9300_sds_cmu_page_get(sds); */
	page = 0x25; /* 10GR and 1000BX */
	sds = (sds % 2) ? (sds - 1) : (sds);

	rtpcs_sds_write_bits(ctrl, sds, page, 0x1c, 15, 15, 1);
	rtpcs_sds_write_bits(ctrl, sds + 1, page, 0x1c, 15, 15, 1);

	en = rtpcs_sds_read_bits(ctrl, sds, page, 27, 1, 1);
	if(!en) { /* Auto mode */
		rtpcs_sds_write(ctrl, sds, 0x1f, 0x02, 31);

		cmu_band = rtpcs_sds_read_bits(ctrl, sds, 0x1f, 0x15, 5, 1);
	} else {
		cmu_band = rtpcs_sds_read_bits(ctrl, sds, page, 30, 4, 0);
	}

	return cmu_band;
}

static int rtpcs_930x_setup_serdes(struct rtpcs_ctrl *ctrl, int sds,
				   phy_interface_t phy_mode)
{
	int calib_tries = 0;

	if (sds < 0 || sds > 11)
		return -EINVAL;

	/* Rely on setup from U-boot for some modes, e.g. USXGMII */
	switch (phy_mode) {
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_10G_QXGMII:
		break;
	default:
		return 0;
	}

	/* Turn Off Serdes */
	rtpcs_930x_sds_set(ctrl, sds, RTL930X_SDS_OFF);

	/* Apply serdes patches */
	rtpcs_930x_sds_patch(ctrl, sds, phy_mode);

	/* Maybe use dal_longan_sds_init */

	/* dal_longan_construct_serdesConfig_init */ /* Serdes Construct */
	rtpcs_930x_phy_enable_10g_1g(ctrl, sds);

	/* ----> dal_longan_sds_mode_set */
	pr_info("%s: Configuring RTL9300 SERDES %d\n", __func__, sds);

	/* Set SDS polarity */
	rtpcs_930x_sds_set_polarity(ctrl, sds, ctrl->tx_pol_inv[sds],
				    ctrl->rx_pol_inv[sds]);

	/* Enable SDS in desired mode */
	rtpcs_930x_sds_mode_set(ctrl, sds, phy_mode);

	/* Enable Fiber RX */
	rtpcs_sds_write_bits(ctrl, sds, 0x20, 2, 12, 12, 0);

	/* Calibrate SerDes receiver in loopback mode */
	rtpcs_930x_sds_10g_idle(ctrl, sds);
	do {
		rtpcs_930x_sds_do_rx_calibration(ctrl, sds, phy_mode);
		calib_tries++;
		mdelay(50);
	} while (rtpcs_930x_sds_check_calibration(ctrl, sds, phy_mode) && calib_tries < 3);
	if (calib_tries >= 3)
		pr_warn("%s: SerDes RX calibration failed\n", __func__);

	/* Leave loopback mode */
	rtpcs_930x_sds_tx_config(ctrl, sds, phy_mode);

	return 0;
}

/* RTL931X */

static void rtpcs_931x_sds_reset(struct rtpcs_ctrl *ctrl, u32 sds)
{
	u32 o, v, o_mode;
	int shift = ((sds & 0x3) << 3);

	/* TODO: We need to lock this! */

	regmap_read(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, &o);
	v = o | BIT(sds);
	regmap_write(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, v);

	regmap_read(ctrl->map, RTL931X_SERDES_MODE_CTRL + 4 * (sds >> 2), &o_mode);
	v = BIT(7) | 0x1F;
	regmap_write_bits(ctrl->map, RTL931X_SERDES_MODE_CTRL + 4 * (sds >> 2),
			  0xff << shift, v << shift);
	regmap_write(ctrl->map, RTL931X_SERDES_MODE_CTRL + 4 * (sds >> 2), o_mode);

	regmap_write(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, o);
}

static void rtpcs_931x_sds_10gr_symErr_get(struct rtpcs_ctrl *ctrl,
					   u32 sds,
					   enum rtpcs_sds_mode mode,
					   struct rtpcs_symerr *sym_err_info)
{
	switch (mode) {
	case RTPCS_SDS_MODE_10GBASER:
		// ch[0] =
		sym_err_info->ch[0] = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x5), 1, 7, 0);
		break;
	case RTPCS_SDS_MODE_1000BASEX:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_DSDS_PAGE(0x1), 24, 2, 0, 0x0);
		// v=
		sym_err_info->ch[0] = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_DSDS_PAGE(0x1), 3, 15, 8);
		// ch[0] = (v << 16) | v2=
		sym_err_info->ch[0] <<= 16;
		sym_err_info->ch[0] |= rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_DSDS_PAGE(0x1), 2, 15, 0);

		// clear symerr
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_DSDS_PAGE(0x1), 3, 15, 8, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_DSDS_PAGE(0x1), 2, 15, 8, 0x0);
		break;
	default:
		break;
	}
}

static void rtpcs_931x_sds_symerr_clear(struct rtpcs_ctrl *ctrl, u32 sds,
					enum rtpcs_sds_mode mode)
{
	struct rtpcs_symerr info = {0};
	switch (mode) {
	case RTPCS_SDS_MODE_XSGMII:
	case RTPCS_SDS_MODE_HISGMII:
	case RTPCS_SDS_MODE_SGMII:
		for (int i = 0; i < 4; ++i) {
			rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 24,  2, 0, i);
			rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1),  3, 15, 8, 0x0);
			rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1),  2, 15, 0, 0x0);
		}

		for (int i = 0; i < 4; ++i) {
			rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x1), 24,  2, 0, i);
			rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x1),  3, 15, 8, 0x0);
			rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x1),  2, 15, 0, 0x0);
		}

		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 0, 15, 0, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 1, 15, 8, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x1), 0, 15, 0, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x1), 1, 15, 8, 0x0);
		break;
	default:
		rtpcs_931x_sds_10gr_symErr_get(ctrl, sds, mode, &info);
		break;
	}

	return;
}

static int rtpcs_931x_sds_get_cmu_page(enum rtpcs_sds_mode mode)
{
	switch (mode) {
	case RTPCS_SDS_MODE_SGMII:
	case RTPCS_SDS_MODE_1000BASEX:
		return 0x24;
	case RTPCS_SDS_MODE_HISGMII:
	case RTPCS_SDS_MODE_2500BASEX:
		return 0x28;
	case RTPCS_SDS_MODE_QSGMII:
		return 0x34;	// return 0x2a;
	case RTPCS_SDS_MODE_XSGMII:
	case RTPCS_SDS_MODE_QHSGMII:
	case RTPCS_SDS_MODE_USXGMII_10GSXGMII:
	case RTPCS_SDS_MODE_USXGMII_10GDXGMII:
	case RTPCS_SDS_MODE_USXGMII_10GQXGMII:
	case RTPCS_SDS_MODE_10GBASER:
		return 0x2e;
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static int rtpcs_931x_sds_set_cmu_type(struct rtpcs_ctrl *ctrl, u32 sds,
				       enum rtpcs_sds_mode mode, int chiptype)
{
	u32 frc_cmu_spd, frc_lc_mode_bitnum, frc_lc_mode_val_bitnum;
	enum rtpcs_sds_cmu_type cmu_type = RTPCS_SDS_CMU_NONE;
	u32 even_sds, lane;
	u32 cmu_page = 0;

	switch (mode) {
	case RTPCS_SDS_MODE_OFF:
	case RTPCS_SDS_MODE_10GBASER:
	case RTPCS_SDS_MODE_XSGMII:
	case RTPCS_SDS_MODE_USXGMII_10GSXGMII:
	case RTPCS_SDS_MODE_USXGMII_10GDXGMII:
	case RTPCS_SDS_MODE_USXGMII_10GQXGMII:
	case RTPCS_SDS_MODE_USXGMII_5GSXGMII:
	case RTPCS_SDS_MODE_USXGMII_5GDXGMII:
	case RTPCS_SDS_MODE_USXGMII_2_5GSXGMII:
		return 0;

	case RTPCS_SDS_MODE_1000BASEX:
	case RTPCS_SDS_MODE_SGMII:
	case RTPCS_SDS_MODE_QSGMII:
		cmu_type = RTPCS_SDS_CMU_RING;
		frc_cmu_spd = 0;
		break;

	case RTPCS_SDS_MODE_2500BASEX:
	case RTPCS_SDS_MODE_HISGMII:
		cmu_type = RTPCS_SDS_CMU_RING;
		frc_cmu_spd = 1;
		break;

/*	case MII_10GR1000BX_AUTO:
		if (chiptype)
			rtpcs_sds_write_bits(ctrl, sds, 0x24, 0xd, 14, 14, 0);
		return; */

	default:
		pr_info("SerDes %d mode is invalid\n", sds);
		return -EINVAL;
	}

	lane = sds % 2;
	if (lane == 0) {
		frc_lc_mode_bitnum = 4;
		frc_lc_mode_val_bitnum = 5;
	} else {
		frc_lc_mode_bitnum = 6;
		frc_lc_mode_val_bitnum = 7;
	}

	even_sds = sds - lane;

	pr_info("%s: cmu_type %0d cmu_page %x frc_cmu_spd %d lane %d sds %d\n",
	        __func__, cmu_type, cmu_page, frc_cmu_spd, lane, sds);

	cmu_page = rtpcs_931x_sds_get_cmu_page(mode);
	if (cmu_type == RTPCS_SDS_CMU_RING) {
		rtpcs_sds_write_bits(ctrl, sds, cmu_page, 0x7, 15, 15, 0);
		if (chiptype)
			rtpcs_sds_write_bits(ctrl, sds, cmu_page, 0xd, 14, 14, 0);

		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, 3, 2, 0x3);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, frc_lc_mode_bitnum, frc_lc_mode_bitnum, 0x1);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, frc_lc_mode_val_bitnum, frc_lc_mode_val_bitnum, 0x0);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, 12, 12, 0x1);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, 15, 13, frc_cmu_spd);
	} else if (cmu_type == RTPCS_SDS_CMU_LC) {
		rtpcs_sds_write_bits(ctrl, sds, cmu_page, 0x7, 15, 15, 1);
		if (chiptype)
			rtpcs_sds_write_bits(ctrl, sds, cmu_page, 0xd, 14, 14, 1);

		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, 1, 0, 0x3);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, frc_lc_mode_bitnum, frc_lc_mode_bitnum, 0x1);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, frc_lc_mode_val_bitnum, frc_lc_mode_val_bitnum, 0x1);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, 8, 8, 0x1);
		rtpcs_sds_write_bits(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x20), 0x12, 11, 9, frc_cmu_spd);
	}

	return 0;
}

static void rtpcs_931x_sds_rx_reset(struct rtpcs_ctrl *ctrl, u32 sds)
{
	if (sds < 2)
		return;

	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x12, 0x2740);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x0, 0x0);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x2, 0x2010);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 0xc10);

	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x12, 0x27c0);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x0, 0xc000);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x2, 0x6010);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 0xc30);

	mdelay(50);
}

static int rtpcs_931x_sds_mii_set_mode(struct rtpcs_ctrl *ctrl, u32 sds,
				       enum rtpcs_sds_mode mode)
{
	u32 val;

	switch (mode) {
	case RTPCS_SDS_MODE_OFF:
		val = 0x1f;
		break;
	case RTPCS_SDS_MODE_1000BASEX:
		val = 0x04;
		break;
	case RTPCS_SDS_MODE_2500BASEX:
		val = 0x16;
		break;
	case RTPCS_SDS_MODE_10GBASER:
		val = 0x1a;
		break;
	case RTPCS_SDS_MODE_SGMII:
		val = 0x02;
		break;
	case RTPCS_SDS_MODE_HISGMII:
		val = 0x12;
		break;
	case RTPCS_SDS_MODE_QSGMII:
		val = 0x06;
		break;
	case RTPCS_SDS_MODE_QHSGMII:
		val = 0x11;
		break;
	case RTPCS_SDS_MODE_XSGMII:
		val = 0x10;
		break;
	case RTPCS_SDS_MODE_USXGMII_10GSXGMII:
	case RTPCS_SDS_MODE_USXGMII_10GDXGMII:
	case RTPCS_SDS_MODE_USXGMII_10GQXGMII:
	case RTPCS_SDS_MODE_USXGMII_5GSXGMII:
	case RTPCS_SDS_MODE_USXGMII_5GDXGMII:
	case RTPCS_SDS_MODE_USXGMII_2_5GSXGMII:
		val = 0x0d;
		break;
	default:
		return -EINVAL;
	}

	val |= (1 << 7); /* force mode bit */
	return regmap_write(ctrl->map, RTL931X_SERDES_MODE_CTRL + 4 * (sds >> 2), val);
}

static int rtpcs_931x_sds_disable(struct rtpcs_ctrl *ctrl, u32 sds)
{
	return rtpcs_931x_sds_mii_set_mode(ctrl, sds, RTPCS_SDS_MODE_OFF);
}

static int rtpcs_931x_sds_fiber_set_mode(struct rtpcs_ctrl *ctrl, u32 sds,
					 enum rtpcs_sds_mode mode)
{
	u32 val;

	/* clear symbol error count before changing mode */
	rtpcs_931x_sds_symerr_clear(ctrl, sds, mode);
	rtpcs_931x_sds_disable(ctrl, sds);

	switch (mode) {
	case RTPCS_SDS_MODE_OFF:
		val = 0x3f;
		break;
	case RTPCS_SDS_MODE_1000BASEX: /* serdes mode FIBER1G */
		val = 0x09;
		break;
	case RTPCS_SDS_MODE_10GBASER:
		val = 0x35;
		break;

	case RTPCS_SDS_MODE_SGMII:
		val = 0x5;
		break;
	case RTPCS_SDS_MODE_USXGMII_10GSXGMII:
        case RTPCS_SDS_MODE_USXGMII_10GDXGMII:
        case RTPCS_SDS_MODE_USXGMII_10GQXGMII:
        case RTPCS_SDS_MODE_USXGMII_5GSXGMII:
        case RTPCS_SDS_MODE_USXGMII_5GDXGMII:
        case RTPCS_SDS_MODE_USXGMII_2_5GSXGMII:
		val = 0x1b;
		break;
	default:
		val = 0x25;
	}

	pr_info("%s writing analog SerDes Mode value %02x\n", __func__, val);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x1f), 0x9, 11, 6, val);

	return 0;
}

static int rtpcs_931x_sds_set_mode(struct rtpcs_ctrl *ctrl, u32 sds,
				   enum rtpcs_sds_mode mode)
{
	if (mode == RTPCS_SDS_MODE_XSGMII)
		return rtpcs_931x_sds_mii_set_mode(ctrl, sds, mode);
	else
		return rtpcs_931x_sds_fiber_set_mode(ctrl, sds, mode);
}

static int rtpcs_931x_sds_fiber_disable(struct rtpcs_ctrl *ctrl, u32 sds)
{
	return rtpcs_931x_sds_fiber_set_mode(ctrl, sds, RTPCS_SDS_MODE_OFF);
}

__attribute__((unused))
static int rtpcs_931x_sds_get_cmu_band(struct rtpcs_ctrl *ctrl, int sds,
				       enum rtpcs_sds_mode mode)
{
	int even_sds, page;
	u32 band;

	even_sds = sds & ~1;
	page = rtpcs_931x_sds_get_cmu_page(mode);
	page += 1;

	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x1f), 0x2, 73);
	rtpcs_sds_write_bits(ctrl, sds, page, RTPCS_931X_ASDS_PAGE(0x5), 15, 15, 0x1);

	band = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x1f), 0x15, 8, 3);
	pr_info("%s: sds %u band is %u\n", __func__, sds, band);
	return band;
}

static int rtpcs_931x_sds_set_cmu_band(struct rtpcs_ctrl *ctrl, int sds,
				       bool enable, u32 band,
				       enum rtpcs_sds_mode mode)
{
	int even_sds, page;
	u32 en_val;

	even_sds = sds & ~1;
	page = rtpcs_931x_sds_get_cmu_page(mode);
	page += 1;

	en_val = enable ? 0 : 1;
	rtpcs_sds_write_bits(ctrl, sds, page, RTPCS_931X_ASDS_PAGE(0x7), 13, 13, en_val);
	rtpcs_sds_write_bits(ctrl, sds, page, RTPCS_931X_ASDS_PAGE(0x7), 11, 11, en_val);

	rtpcs_sds_write_bits(ctrl, sds, page, RTPCS_931X_ASDS_PAGE(0x7), 4, 0, band);

	rtpcs_931x_sds_reset(ctrl, sds);
	return 0;
}

// __attribute__((unused))
static struct rtpcs_link_sts rtpcs_931x_sds_link_sts_get(struct rtpcs_ctrl *ctrl, u32 sds, enum rtpcs_sds_mode sds_mode)
{
	struct rtpcs_link_sts link_sts;
	switch(sds_mode) {
	case RTPCS_SDS_MODE_XSGMII:
		link_sts.sts = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 29, 8, 0);
		link_sts.sts1 = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x1), 29, 8, 0);
		link_sts.latch_sts = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 30, 8, 0);
		link_sts.latch_sts1 = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x1), 30, 8, 0);
		break;
	case RTPCS_SDS_MODE_HISGMII:
	case RTPCS_SDS_MODE_SGMII:
		link_sts.sts = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 29, 8, 0);
		link_sts.latch_sts = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 30, 8, 0);
		break;
	default:
		link_sts.sts = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x5), 0, 12, 12);
		link_sts.latch_sts = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x4), 1, 2, 2);
		link_sts.latch_sts1 = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x2), 1, 2, 2);
		link_sts.sts1 = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x2), 1, 2, 2);
		break;
	}

	pr_info("%s: serdes %d sts %d, sts1 %d, latch_sts %d, latch_sts1 %d\n", __func__,
		sds, link_sts.sts, link_sts.sts1, link_sts.latch_sts, link_sts.latch_sts1);

	return link_sts;
}

__attribute__((unused))
static int rtpcs_931x_sds_set_polarity(struct rtpcs_ctrl *ctrl, u32 sds,
				       bool tx_inv, bool rx_inv)
{
	u8 rx_val = rx_inv ? 1 : 0;
	u8 tx_val = tx_inv ? 1 : 0;
	u32 val;
	int ret;

	/* 10gr_*_inv */
	val = (tx_val << 1) | rx_val;
	ret = rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x2, 14, 13, val);
	if (ret)
		return ret;

	/* xsg_*_inv */
	val = (rx_val << 1) | tx_val;
	ret = rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x0), 0x0, 9, 8, val);
	if (ret)
		return ret;

	return rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x0), 0x0, 9, 8, val);
}

static sds_config rtpcs_931x_sds_cfg_ana_common[] = {
	{ 0x21, 0x00, 0x1800 }, { 0x21, 0x01, 0x0060 }, { 0x21, 0x02, 0x3000 },
	{ 0x21, 0x03, 0xFFFF }, { 0x21, 0x04, 0x0603 }, { 0x21, 0x05, 0x1104 },
	{ 0x21, 0x06, 0x4444 }, { 0x21, 0x07, 0x7044 },	{ 0x21, 0x08, 0xF104 },
	{ 0x21, 0x09, 0xF104 }, { 0x21, 0x0A, 0xF104 }, { 0x21, 0x0B, 0x0003 },
	{ 0x21, 0x0C, 0x007F }, { 0x21, 0x0D, 0x3FE4 }, { 0x21, 0x0E, 0x31F9 },
	{ 0x21, 0x0F, 0x0618 }, { 0x21, 0x10, 0x1FF8 }, { 0x21, 0x11, 0x7C9F },
	{ 0x21, 0x12, 0x7C9F }, { 0x21, 0x13, 0x13FF }, { 0x21, 0x14, 0x001F },
	{ 0x21, 0x15, 0x01F0 }, { 0x21, 0x16, 0x1067 }, { 0x21, 0x17, 0x8AF1 },
	{ 0x21, 0x18, 0x210A }, { 0x21, 0x19, 0xF0F0 }
};

static sds_config rtpcs_931x_sds_cfg_10p3125g[] = {
	{ 0x2E, 0x00, 0x0107 }, { 0x2E, 0x01, 0x0200 }, { 0x2E, 0x02, 0x6A24 },
	{ 0x2E, 0x03, 0xD10D }, { 0x2E, 0x04, 0xD550 }, { 0x2E, 0x05, 0xA95E },
	{ 0x2E, 0x06, 0xE31D }, { 0x2E, 0x07, 0x000E }, { 0x2E, 0x08, 0x0294 },
	{ 0x2E, 0x09, 0x0CE4 }, { 0x2E, 0x0a, 0x7FC8 }, { 0x2E, 0x0b, 0xE0E7 },
	{ 0x2E, 0x0c, 0x0200 }, { 0x2E, 0x0d, 0xDF80 }, { 0x2E, 0x0e, 0x0000 },
	{ 0x2E, 0x0f, 0x1FC4 }, { 0x2E, 0x10, 0x0C3F }, { 0x2E, 0x11, 0x0000 },
	{ 0x2E, 0x12, 0x27C0 }, { 0x2E, 0x13, 0x7F1C }, { 0x2E, 0x14, 0x1300 },
	{ 0x2E, 0x15, 0x003F }, { 0x2E, 0x16, 0xBE7F }, { 0x2E, 0x17, 0x0090 },
	{ 0x2E, 0x18, 0x0000 }, { 0x2E, 0x19, 0x4000 }, { 0x2E, 0x1a, 0x0000 },
	{ 0x2E, 0x1b, 0x8000 }, { 0x2E, 0x1c, 0x011E }, { 0x2E, 0x1d, 0x0000 },
	{ 0x2E, 0x1e, 0xC8FF }, { 0x2E, 0x1f, 0x0000 }, { 0x2F, 0x00, 0xC000 },
	{ 0x2F, 0x01, 0xF000 }, { 0x2F, 0x02, 0x6010 }, { 0x2F, 0x12, 0x0EEE },
	{ 0x2F, 0x13, 0x0000 }, { 0x06, 0x00, 0x0000 }
};

static sds_config rtpcs_931x_sds_cfg_10p3125g_cmu[] = {
	{ 0x2F, 0x03, 0x4210 }, { 0x2F, 0x04, 0x0000 }, { 0x2F, 0x05, 0x3FD9 },
	{ 0x2F, 0x06, 0x58A6 }, { 0x2F, 0x07, 0x2990 }, { 0x2F, 0x08, 0xFFF4 },
	{ 0x2F, 0x09, 0x1F08 }, { 0x2F, 0x0A, 0x0000 }, { 0x2F, 0x0B, 0x8000 },
	{ 0x2F, 0x0C, 0x4224 }, { 0x2F, 0x0D, 0x0000 }, { 0x2F, 0x0E, 0x0400 },
	{ 0x2F, 0x0F, 0xA464 }, { 0x2F, 0x10, 0x8000 }, { 0x2F, 0x11, 0x0165 },
	{ 0x20, 0x11, 0x000D }, { 0x20, 0x12, 0x510F }, { 0x20, 0x00, 0x0030 }
};

static sds_config rtpcs_931x_sds_cfg_ana_2p5g[] = {
	{0x26, 0x00, 0x0104}, {0x26, 0x01, 0x0200}, {0x26, 0x02, 0x2A24},
	{0x26, 0x03, 0xD10D}, {0x26, 0x04, 0xD550}, {0x26, 0x05, 0xA95E},
	{0x26, 0x06, 0xE31D}, {0x26, 0x07, 0x000E}, {0x26, 0x08, 0x0294},
	{0x26, 0x09, 0x04E4}, {0x26, 0x0A, 0x7FC8}, {0x26, 0x0B, 0xE0E7},
	{0x26, 0x0C, 0x0200}, {0x26, 0x0D, 0xDF80}, {0x26, 0x0E, 0x0800},
	{0x26, 0x0F, 0x1FD8}, {0x26, 0x10, 0x0C3F}, {0x26, 0x11, 0x0000},
	{0x26, 0x12, 0x27C0}, {0x26, 0x13, 0x7F1C}, {0x26, 0x14, 0x1300},
	{0x26, 0x15, 0x003F}, {0x26, 0x16, 0xBE7F}, {0x26, 0x17, 0x0090},
	{0x26, 0x18, 0x0000}, {0x26, 0x19, 0x407F}, {0x26, 0x1A, 0x0000},
	{0x26, 0x1B, 0x8000}, {0x26, 0x1C, 0x011E}, {0x26, 0x1D, 0x0000},
	{0x26, 0x1E, 0xC8FF}, {0x26, 0x1F, 0x0000}, {0x27, 0x00, 0xC000},
	{0x27, 0x01, 0xF000}, {0x27, 0x02, 0x6010}, {0x27, 0x03, 0x6410},
	{0x27, 0x05, 0x27D9}, {0x27, 0x07, 0x2990}, {0x27, 0x08, 0xFFF4},
	{0x27, 0x09, 0x3082}, {0x27, 0x0C, 0x6424}, {0x27, 0x11, 0x037B},
	{0x27, 0x12, 0x0EEE}, {0x27, 0x13, 0x0000}
};

static sds_config rtpcs_931x_sds_cfg_ana_1p25g[] =
{
	{0x24, 0x00, 0x0104}, {0x24, 0x01, 0x0200}, {0x24, 0x02, 0x2A24},
	{0x24, 0x03, 0xD10D}, {0x24, 0x04, 0xD550}, {0x24, 0x05, 0xA95E},
	{0x24, 0x06, 0xE31D}, {0x24, 0x07, 0x800E}, {0x24, 0x08, 0x0294},
	{0x24, 0x09, 0x04E4}, {0x24, 0x0A, 0x7FC8}, {0x24, 0x0B, 0xE0E7},
	{0x24, 0x0C, 0x0200}, {0x24, 0x0D, 0x9F80}, {0x24, 0x0E, 0x0000},
	{0x24, 0x0F, 0x1FF0}, {0x24, 0x10, 0x0C3F}, {0x24, 0x11, 0x0000},
	{0x24, 0x12, 0x27C0}, {0x24, 0x13, 0x7F1C}, {0x24, 0x14, 0x1300},
	{0x24, 0x15, 0x003F}, {0x24, 0x16, 0xBE7F}, {0x24, 0x17, 0x0090},
	{0x24, 0x18, 0x0000}, {0x24, 0x19, 0x407F}, {0x24, 0x1A, 0x0000},
	{0x24, 0x1B, 0x8000}, {0x24, 0x1C, 0x011E}, {0x24, 0x1D, 0x0000},
	{0x24, 0x1E, 0xC8FF}, {0x24, 0x1F, 0x0000}, {0x25, 0x00, 0xC000},
	{0x25, 0x01, 0xF000}, {0x25, 0x02, 0x6010}, {0x25, 0x12, 0x0EEE},
	{0x25, 0x13, 0x0000}
};

static sds_config rtpcs_931x_sds_cfg_ana2[] = {
	{0x20, 0x12, 0x150F},
	{0x2E, 0x7 , 0x800E},
	{0x2A, 0x7 , 0x800E},
	{0x24, 0x7 , 0x000E},
	{0x26, 0x7 , 0x000E},
	{0x28, 0x7 , 0x000E},

	{0x2F, 0x12, 0x0AAA},

	{0x2A, 0x12, 0x2740},
	{0x2B, 0x0 , 0x0   },
	{0x2B, 0x2 , 0x2010},

	{0x2F, 0x3 , 0x84A0},
	{0x2F, 0xC , 0x84A4},

	{0x24, 0xD , 0xDF80},
	{0x2A, 0xD , 0xDF80},

	{0x2F, 0x5 , 0x2FD9},
	{0x2F, 0x5 , 0x3FD9},
	{0x21, 0x16, 0x1065},
	{0x21, 0x16, 0x1067},

	{0x21, 0x19, 0xF0A5},
};

static sds_config rtpcs_931x_sds_cfg_ana_common_type1[] = {
	{0x21, 0x00, 0x1800}, {0x21, 0x01, 0x0060}, {0x21, 0x02, 0x3000},
	{0x21, 0x03, 0xFFFF}, {0x21, 0x04, 0x0603}, {0x21, 0x05, 0x1104},
	{0x21, 0x06, 0x4444}, {0x21, 0x07, 0x7044}, {0x21, 0x08, 0xF104},
	{0x21, 0x09, 0xF104}, {0x21, 0x0A, 0xF104}, {0x21, 0x0B, 0x0003},
	{0x21, 0x0C, 0x007F}, {0x21, 0x0D, 0x3FE4}, {0x21, 0x0E, 0x31F9},
	{0x21, 0x0F, 0x0618}, {0x21, 0x10, 0x1FF8}, {0x21, 0x11, 0x7C9F},
	{0x21, 0x12, 0x7C9F}, {0x21, 0x13, 0x13FF}, {0x21, 0x14, 0x001F},
	{0x21, 0x15, 0x01F0}, {0x21, 0x16, 0x1064}, {0x21, 0x17, 0x8AF1},
	{0x21, 0x18, 0x210A}, {0x21, 0x19, 0xF0F1}
};

static sds_config rtpcs_931x_sds_cfg_ana_10p3125g_type1[] = {
	{ 0x2E, 0x00, 0x0107 }, { 0x2E, 0x01, 0x01A3 }, { 0x2E, 0x02, 0x6A24 },
	{ 0x2E, 0x03, 0xD10D }, { 0x2E, 0x04, 0x8000 }, { 0x2E, 0x05, 0xA17E },
	{ 0x2E, 0x06, 0xE31D }, { 0x2E, 0x07, 0x800E }, { 0x2E, 0x08, 0x0294 },
	{ 0x2E, 0x09, 0x0CE4 }, { 0x2E, 0x0A, 0x7FC8 }, { 0x2E, 0x0B, 0xE0E7 },
	{ 0x2E, 0x0C, 0x0200 }, { 0x2E, 0x0D, 0xDF80 }, { 0x2E, 0x0E, 0x0000 },
	{ 0x2E, 0x0F, 0x1FC2 }, { 0x2E, 0x10, 0x0C3F }, { 0x2E, 0x11, 0x0000 },
	{ 0x2E, 0x12, 0x27C0 }, { 0x2E, 0x13, 0x7E1D }, { 0x2E, 0x14, 0x1300 },
	{ 0x2E, 0x15, 0x003F }, { 0x2E, 0x16, 0xBE7F }, { 0x2E, 0x17, 0x0090 },
	{ 0x2E, 0x18, 0x0000 }, { 0x2E, 0x19, 0x4000 }, { 0x2E, 0x1A, 0x0000 },
	{ 0x2E, 0x1B, 0x8000 }, { 0x2E, 0x1C, 0x011F }, { 0x2E, 0x1D, 0x0000 },
	{ 0x2E, 0x1E, 0xC8FF }, { 0x2E, 0x1F, 0x0000 }, { 0x2F, 0x00, 0xC000 },
	{ 0x2F, 0x01, 0xF000 }, { 0x2F, 0x02, 0x6010 }, { 0x2F, 0x12, 0x0EE7 },
	{ 0x2F, 0x13, 0x0000 }
};

static sds_config rtpcs_931x_sds_cfg_ana_10p3125g_cmu_type1[] = {
	{ 0x2F, 0x03, 0x4210 }, { 0x2F, 0x04, 0x0000 }, { 0x2F, 0x05, 0x0019 },
	{ 0x2F, 0x06, 0x18A6 }, { 0x2F, 0x07, 0x2990 }, { 0x2F, 0x08, 0xFFF4 },
	{ 0x2F, 0x09, 0x1F08 }, { 0x2F, 0x0A, 0x0000 }, { 0x2F, 0x0B, 0x8000 },
	{ 0x2F, 0x0C, 0x4224 }, { 0x2F, 0x0D, 0x0000 }, { 0x2F, 0x0E, 0x0000 },
	{ 0x2F, 0x0F, 0xA470 }, { 0x2F, 0x10, 0x8000 }, { 0x2F, 0x11, 0x037B }
};

static sds_config rtpcs_931x_sds_cfg_ana_2p5g_type1[] = {
	{0x26, 0x00, 0xF904}, {0x26, 0x01, 0x0200}, {0x26, 0x02, 0x2A20},
	{0x26, 0x03, 0xD10D}, {0x26, 0x04, 0x8000}, {0x26, 0x05, 0xA17E},
	{0x26, 0x06, 0xE115}, {0x26, 0x07, 0x000E}, {0x26, 0x08, 0x0294},
	{0x26, 0x09, 0x04E4}, {0x26, 0x0A, 0x7FC8}, {0x26, 0x0B, 0xE0E7},
	{0x26, 0x0C, 0x0200}, {0x26, 0x0D, 0xDF80}, {0x26, 0x0E, 0x0000},
	{0x26, 0x0F, 0x1FE0}, {0x26, 0x10, 0x0C3F}, {0x26, 0x11, 0x0000},
	{0x26, 0x12, 0x27C0}, {0x26, 0x13, 0x7E1D}, {0x26, 0x14, 0x1300},
	{0x26, 0x15, 0x003F}, {0x26, 0x16, 0xBE7F}, {0x26, 0x17, 0x0090},
	{0x26, 0x18, 0x0000}, {0x26, 0x19, 0x407F}, {0x26, 0x1A, 0x0000},
	{0x26, 0x1B, 0x8000}, {0x26, 0x1C, 0x011E}, {0x26, 0x1D, 0x0000},
	{0x26, 0x1E, 0xC8FF}, {0x26, 0x1F, 0x0000}, {0x27, 0x00, 0xC000},
	{0x27, 0x01, 0xF000}, {0x27, 0x02, 0x6010}, {0x27, 0x12, 0x0EE7},
	{0x27, 0x13, 0x0000}
};

static sds_config rtpcs_931x_sds_cfg_ana_1p25g_type1[] = {
	{0x24, 0x00, 0xF904}, {0x24, 0x01, 0x0200}, {0x24, 0x02, 0x2A20},
	{0x24, 0x03, 0xD10D}, {0x24, 0x04, 0x8000}, {0x24, 0x05, 0xA17E},
	{0x24, 0x06, 0xE115}, {0x24, 0x07, 0x000E}, {0x24, 0x08, 0x0294},
	{0x24, 0x09, 0x84E4}, {0x24, 0x0A, 0x7FC8}, {0x24, 0x0B, 0xE0E7},
	{0x24, 0x0C, 0x0200}, {0x24, 0x0D, 0xDF80}, {0x24, 0x0E, 0x0000},
	{0x24, 0x0F, 0x1FF0}, {0x24, 0x10, 0x0C3F}, {0x24, 0x11, 0x0000},
	{0x24, 0x12, 0x27C0}, {0x24, 0x13, 0x7E1D}, {0x24, 0x14, 0x1300},
	{0x24, 0x15, 0x003F}, {0x24, 0x16, 0xBE7F}, {0x24, 0x17, 0x0090},
	{0x24, 0x18, 0x0000}, {0x24, 0x19, 0x407F}, {0x24, 0x1A, 0x0000},
	{0x24, 0x1B, 0x8000}, {0x24, 0x1C, 0x011E}, {0x24, 0x1D, 0x0000},
	{0x24, 0x1E, 0xC8FF}, {0x24, 0x1F, 0x0000}, {0x25, 0x00, 0xC000},
	{0x25, 0x01, 0xF000}, {0x25, 0x02, 0x6010}, {0x25, 0x12, 0x0EE7},
	{0x25, 0x13, 0x0000}
};


static void rtpcs_931x_init_leq_dfe(struct rtpcs_ctrl *ctrl, int sds)
{
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xd, 6, 0, 0x0);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xd, 7, 7, 0x1);

	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1c, 5, 0, 0x1e);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1d, 11, 0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1f, 11, 0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x0, 11, 0, 0x00);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x1, 11, 0, 0x00);

	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 12, 6, 0x7f);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x12, 0xaaa);
}

static int rtpcs_931x_sds_config_usxgmii(struct rtpcs_ctrl *ctrl, int sds, int chiptype)
{
	u32 even_sds = sds & ~1;
	u32 op_code, am_period;

	if (chiptype) {
		rtpcs_sds_write_bits(ctrl, sds, 0x6, 0x2, 12, 12, 1);

		for (int i = 0; i < sizeof(rtpcs_931x_sds_cfg_ana_10p3125g_type1) / sizeof(sds_config); ++i) {
			rtpcs_sds_write(ctrl, sds,
					rtpcs_931x_sds_cfg_ana_10p3125g_type1[i].page - 0x4,
					rtpcs_931x_sds_cfg_ana_10p3125g_type1[i].reg,
					rtpcs_931x_sds_cfg_ana_10p3125g_type1[i].data);

		}

		for (int i = 0; i < sizeof(rtpcs_931x_sds_cfg_ana_10p3125g_cmu_type1) / sizeof(sds_config); ++i) {
			rtpcs_sds_write(ctrl, even_sds,
					rtpcs_931x_sds_cfg_ana_10p3125g_cmu_type1[i].page - 0x4,
					rtpcs_931x_sds_cfg_ana_10p3125g_cmu_type1[i].reg,
					rtpcs_931x_sds_cfg_ana_10p3125g_cmu_type1[i].data);
		}

		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x2, 12, 12, 0);
	} else {
		rtpcs_931x_init_leq_dfe(ctrl, sds);
		rtpcs_931x_sds_rx_reset(ctrl, sds);


		// TODO: Consider rtl8224qf phy

		// Only implement default switch case
		op_code = 0xAA;
		am_period = 0x5078;
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x1d, 0x0600);

		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x13, 0x0000);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x14, 0x0000);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x15, 0x0000);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x16, 0x0000);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x17, 0x0000);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x18, 0x0000);

		// end of switch-case
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x7), 0x10, 15, 8, 0x60);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x7), 0x10, 7, 0, op_code);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x12, am_period);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x7), 0x6, 0x1401);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x7), 0x8, 0x1401);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x7), 0xa, 0x1401);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x7), 0xc, 0x1401);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0xe, 0x055a);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0x3, 15, 15, 1);
	}
	return 0;
}

#if 1
static uint64_t rtpcs_931x_port_fib_unidir_set(struct rtpcs_ctrl *ctrl, int port, bool is_unidir)
{
	u32 val[2] = {0};
	u64 unidir_sts, val64;

	regmap_bulk_read(ctrl->map, RTL931X_FIB_UNIDIR_CTRL, &val[0], 2);

	unidir_sts = ((u64)val[1] << 32) | (val[0]);
	val64 = unidir_sts & (is_unidir ? ~BIT_ULL(port) : BIT_ULL(port));
	val[1] = val64 >> 32;
	val[0] = (u32)((u64)val64 & 0xffffffff);
	regmap_bulk_write(ctrl->map, RTL931X_FIB_UNIDIR_CTRL, &val64, sizeof(unidir_sts));

	return unidir_sts;
}
#else
static int rtpcs_931x_port_fib_unidir_set(struct rtpcs_ctrl *ctrl, int port, bool is_unidir)
{
	u32 unidir_sts_port;
	u32 val[2];

	regmap_bulk_read(ctrl->map, RTL931X_FIB_UNIDIR_CTRL, &val[0], sizeof(u64));
	pr_err("[hh CC] %s: val[0]:0x%x val[1]:0x%x\n", __func__, val[0], val[1]);
	regmap_fields_read(ctrl->rm_fields[RTL931X_FIB_UNIDIR], port, &unidir_sts_port);
	regmap_fields_write(ctrl->rm_fields[RTL931X_FIB_UNIDIR], port, is_unidir ? 0 : 1);
	pr_err("[hh CC] %s: Completed\n", __func__);
	return unidir_sts_port;
}
#endif

// _phy_rtl9310_linkDown_chk
static int rtpcs_931x_link_down_chk(struct rtpcs_ctrl *ctrl, int port)
{
	u32 dbg_sts = 0, sts_cnt = 0, chk_cnt = 0;
	u64 unidir_sts = rtpcs_931x_port_fib_unidir_set(ctrl, port, 0);
	int ret;

	/* If serdes returns no packet for 3 consecutive times, link down successful */
	/* TODO: Can this be implemented using regmap_field_read_poll_timeout? */
	while (sts_cnt != 3 && chk_cnt < 0xFFFF) {
		// We don't want cached values while polling for change
		regmap_fields_read(ctrl->rm_fields[RTL931X_TX_NO_PKT], port, &dbg_sts);
		if (dbg_sts)
			sts_cnt++;
		else
			sts_cnt = 0;

		chk_cnt++;
	}

	if (sts_cnt != 3) {
		ret = -1;
		goto out;
	}

	ret = 0;
out:
	// restore previous value
	#if 1
	rtpcs_931x_port_fib_unidir_set(ctrl, port, !!(unidir_sts & BIT_ULL(port)));
	#else
	rtpcs_931x_port_fib_unidir_set(ctrl, port, unidir_sts);
	#endif
	return ret;
}


// _drv_rtl9310_portMacForceLink_oper
// drv_rtl9310_portMacForceLink_set
static void rtpcs_931x_port_mac_force_link_set(struct rtpcs_ctrl *ctrl, int port, bool is_enabled, bool linksts)
{
	if (is_enabled) {
		regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_FORCE_LINK],
			port,
			linksts ? BIT(9) : 0);
	}
	regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_FORCE_LINK_EN],
			    port,
			    is_enabled ? 1 : 0);
}

// _phy_rtl9310_sds_init
static int rtpcs_931x_phy_sds_init(struct rtpcs_link *link)
{
	struct rtpcs_ctrl *ctrl = link->ctrl;
	rtpcs_931x_port_mac_force_link_set(ctrl, link->port, true, false);
	link->is_rx_calibrated = false;
	rtpcs_931x_link_down_chk(ctrl, link->port);
	rtpcs_sds_write_bits(ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xe, 13, 11, 0);
	rtpcs_931x_init_leq_dfe(ctrl, link->sds);
	return 0;
}

// _phy_rtl9310_linkDown_process
static void rtpcs_931x_sds_linkdown_process(struct rtpcs_link *link)
{
	regmap_write_bits(link->ctrl->map,
			  RTL931X_ISR_SERDES_RXIDLE,
			  BIT(link->sds - 2),
			  BIT(link->sds - 2));
	rtpcs_931x_phy_sds_init(link);
}

// _phy_rtl9310_dfe_set
static void rtpcs_9310_sds_dfe_set(struct rtpcs_ctrl *ctrl,
				   u32 sds,
				   enum rtpcs_9310_dfe_type dfe_type,
				   int val)
{
	switch(dfe_type) {
	case RTPCS_9310_DFE_VTH:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x12, 11, 4, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 12, 12, 1);
		break;
	case RTPCS_9310_DFE_TAP0:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1c, 5, 5, 0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1c, 4, 0, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 6, 6, 1);
		break;
	case RTPCS_9310_DFE_TAP1EVEN:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1d, 5, 0, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 7, 7, 1);
		break;
	case RTPCS_9310_DFE_TAP1ODD:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1d, 11, 6, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 7, 7, 1);
		break;
	case RTPCS_9310_DFE_TAP2EVEN:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1f, 5, 0, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 8, 8, 1);
		break;
	case RTPCS_9310_DFE_TAP2ODD:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1f, 11, 6, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 8, 8, 1);
		break;
	case RTPCS_9310_DFE_TAP3EVEN:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x0, 5, 0, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 9, 9, 1);
		break;
	case RTPCS_9310_DFE_TAP3ODD:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x0, 11, 6, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 9, 9, 1);
		break;
	case RTPCS_9310_DFE_TAP4EVEN:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x1, 5, 0, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 10, 10, 1);
		break;
	case RTPCS_9310_DFE_TAP4ODD:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x1, 11, 6, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 10, 10, 1);
		break;
	case RTPCS_9310_DFE_FGCAL_OFST:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x19, 14, 7, val);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x19, 6, 6, 1);
		break;
	case RTPCS_9310_DFE_END:
		break;
	}
}

// _phy_rtl9310_fiber_adapt
static int rtpcs_931x_sds_fiber_adapt(struct rtpcs_link *link)
{
	struct rtpcs_9310_dfe sds_dfe[] = {
		{0x0f, 7, 0, 32, 0, RTPCS_9310_DFE_END},
		{0x00, 5, 0, 5, 0, RTPCS_9310_DFE_TAP0},
		{0x0c, 7, 0, 32, 0, RTPCS_9310_DFE_VTH},
	};

	#if 0
	struct rtpcs_9310_dfe sds_dfe2[] = {
		{0x01, 5, 0, 5, 0, RTPCS_9310_DFE_TAP1EVEN},
		{0x06, 5, 0, 5, 0, RTPCS_9310_DFE_TAP1ODD},
		{0x02, 5, 0, 5, 0, RTPCS_9310_DFE_TAP2EVEN},
		{0x07, 5, 0, 5, 0, RTPCS_9310_DFE_TAP2ODD},
		{0x03, 5, 0, 5, 0, RTPCS_9310_DFE_TAP3EVEN},
		{0x08, 5, 0, 5, 0, RTPCS_9310_DFE_TAP3ODD},
		{0x04, 5, 0, 5, 0, RTPCS_9310_DFE_TAP4EVEN},
		{0x09, 5, 0, 5, 0, RTPCS_9310_DFE_TAP4ODD},
	};
	#endif

	struct rtpcs_symerr info = {0};

	if (link->sds < 2)
		return 0;
	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xc, 14, 10, 0);
	pr_info("%s: SDS %d Calibration...\n", __func__, link->sds);
	rtpcs_931x_init_leq_dfe(link->ctrl, link->sds);
	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 6, 6, 0);
	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 12, 12, 0);
	mdelay(200);
	// TODO _phy_rtl9310_dbg_set

	for (int i = 0; i < ARRAY_SIZE(sds_dfe); i++) {
		if (sds_dfe[i].type != RTPCS_9310_DFE_END) {
			if (sds_dfe[i].type == RTPCS_9310_DFE_TAP0)
				sds_dfe[i].val = 31;
			rtpcs_9310_sds_dfe_set(link->ctrl,
					       link->sds,
					       sds_dfe[i].type,
					       sds_dfe[i].val);
		}
	}

	rtpcs_931x_sds_reset(link->ctrl, link->sds);

	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 7, 7, 0);
	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 8, 8, 0);
	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 9, 9, 0);
	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 10, 10, 0);

	// TODO Handle manual DFE config

	for (int i = 0; i < 3; i++) {
		rtpcs_931x_sds_symerr_clear(link->ctrl, link->sds, link->sds_mode);
		mdelay(150);
		rtpcs_931x_sds_10gr_symErr_get(link->ctrl, link->sds, link->sds_mode, &info);
		if (info.ch[0] == 0)
			break;
	}

	if (0 != info.ch[0])
		return -1;
	return 0;
}

// _phy_rtl9310_rxCali
static void rtpcs_931x_sds_rxcali(struct rtpcs_link *link)
{
	u32 ori_off_mode;
	int ret;
	mdelay(50);
	// drv_port_txEnable_set
	regmap_fields_write(link->ctrl->rm_fields[RTL931X_MAC_L2_PORT_TX_EN], link->port, 0);
	// drv_port_rxEnable_set
	regmap_fields_write(link->ctrl->rm_fields[RTL931X_MAC_L2_PORT_RX_EN], link->port, 0);
	regmap_read(link->ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, &ori_off_mode);
	regmap_write_bits(link->ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, BIT(link->sds), BIT(link->sds));
	// if any non-serdes port
	// TODO _phy_rtl9310_leq_adapt
	// else if serdes port
	if (link->sds_mode == RTPCS_SDS_MODE_10GBASER) {
		rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xe, 13, 11, 1);
		ret = rtpcs_931x_sds_fiber_adapt(link);
		if (ret)
			goto error;
	}

	link->is_rx_calibrated = true;

error:
	if (ret)
		rtpcs_931x_phy_sds_init(link);
	regmap_write(link->ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, ori_off_mode);
	regmap_fields_write(link->ctrl->rm_fields[RTL931X_MAC_L2_PORT_TX_EN], link->port, 1);
	regmap_fields_write(link->ctrl->rm_fields[RTL931X_MAC_L2_PORT_RX_EN], link->port, 1);

}

// _phy_rtl9310_linkUp_process
static void rtpcs_931x_sds_linkup_process(struct rtpcs_link *link)
{
	if (!link->is_rx_calibrated) {
		rtpcs_931x_sds_rxcali(link);
	}
	// TODO _phy_rtl9310_linkUp_post_chk
	rtpcs_sds_write_bits(link->ctrl, link->sds, RTPCS_931X_DSDS_PAGE(31), 1, 0, 0, 0x0);
	// TODO dal_mango_stack_linkUp_handler_pre
	rtpcs_931x_port_mac_force_link_set(link->ctrl, link->port, false, true);
	// TODO dal_mango_stack_linkUp_handler_post

}

/*
// phy_rtl9310_dac_chk
static void rtpcs_931x_dac_chk(struct rtpcs_link *link)
{
	switch (link->sds_mode) {

	}
}
*/

// TODO phy_rtl9310_sdsFiberRx_check
/**
 * Returns 0 if fiber_rx is okay
 *
 */
static int rtpcs_931x_sds_fiber_rx_check(struct rtpcs_link *link)
{
	struct rtpcs_ctrl *ctrl = link->ctrl;
	struct rtpcs_symerr symerr_info = {0};
	int check_err_count;

	switch (link->sds_mode) {
	case RTPCS_SDS_MODE_10GBASER:
	case RTPCS_SDS_MODE_1000BASEX:
	case RTPCS_SDS_MODE_2500BASEX:
		break;
	default:
		return 0;
	}

	link->link_sts = rtpcs_931x_sds_link_sts_get(ctrl, link->sds, link->sds_mode);
	if (link->link_sts.sts == 0 &&
	    link->link_sts.sts1 == 0) {
		if (link->is_rx_calibrated) {
			rtpcs_931x_sds_linkdown_process(link);
		}
		return -1;
		/*
		else {
			// TODO _phy_rtl9310_dac_chk
		}
		*/
	}

	if (!link->is_rx_calibrated) {
		rtpcs_931x_sds_linkup_process(link);
	}

	if (link->is_rx_calibrated) {
		for (int i = 0; i < 3; i++) {
			rtpcs_931x_sds_10gr_symErr_get(link->ctrl,
						       link->sds,
						       link->sds_mode,
						       &symerr_info);
			if (symerr_info.ch[0] != 0)
				check_err_count++;

		}
		if (check_err_count >= 2) {
			dev_err(link->ctrl->dev,
				"%s: Fiber RX check port:%d sds:%d: Error in fiber connection.\n",
				__func__,
				link->port,
				link->sds);
			return 1;
		}
	}

	return 0;
}

//TODO dal_phy_fiberRx_watchdog
__attribute__((unused))
static void rtpcs_931x_poll_link_check(struct work_struct *work)
{
	struct rtpcs_ctrl *ctrl = container_of(to_delayed_work(work),
					       struct rtpcs_ctrl,
					       link_check_work);
	int ret;

	for (int port = 0; port < ctrl->cfg->cpu_port; port++) {
		if (!ctrl->link[port])
			continue;
		ret = rtpcs_931x_sds_fiber_rx_check(ctrl->link[port]);
		if (ret != 0) {
			// phy_serdesFiberRx_reset
			rtpcs_931x_sds_linkdown_process(ctrl->link[port]);
			rtpcs_931x_sds_linkup_process(ctrl->link[port]);
		}
		// TODO phy_fiberRx_check
		// TODO phy_fiberRx_reset

		// TODO phy_media_get
	}

	queue_delayed_work(ctrl->wq,
			   &ctrl->link_check_work,
			   RTPCS_SDS_POLL_INTERVAL);
}

static void rtpcs_931x_init_link_check(struct rtpcs_ctrl *ctrl)
{
	pr_err("[hh CC] %s: Work queue init\n", __func__);
	// struct rtpcs_link *link = rtpcs_phylink_pcs_to_link(pcs);
	INIT_DELAYED_WORK(&ctrl->link_check_work, rtpcs_931x_poll_link_check);
	queue_delayed_work(ctrl->wq,
			   &ctrl->link_check_work,
			   RTPCS_SDS_POLL_INTERVAL);
}


static int rtpcs_931x_sds_config_fiber(struct rtpcs_ctrl *ctrl, int sds,
				       int port, enum rtpcs_sds_mode mode)
{
	u32 even_sds = sds & ~1;
	u64 unidir_sts;
	u32 spd_en_ori, spd_ori, ps_sds_sts;
	// , val;
	// u64 val64;

	// u32 xsgmii_sds = rtpcs_931x_sds2xsgmii_sds(sds);
	// u32 analog_sds = rtpcs_931x_get_analog_sds(sds);

	/* from _phy_rtl9310_10gMedia_set */
	unidir_sts = rtpcs_931x_port_fib_unidir_set(ctrl, port, 0);

	rtpcs_931x_port_mac_force_link_set(ctrl, port, true, false);
	// ignore return value, this part is not crucial
	rtpcs_931x_link_down_chk(ctrl, port);
	regmap_fields_read(ctrl->rm_fields[RTL931X_SMI_SPD_SEL], port, &spd_ori);
	regmap_fields_read(ctrl->rm_fields[RTL931X_SMI_FORCE_SPD_EN], port, &spd_en_ori);

	regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_SPD_SEL], port, 0);
	regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_FORCE_SPD_EN], port, 1);

	regmap_read(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, &ps_sds_sts);
	// val = ps_sds_sts | BIT(sds);
	regmap_write_bits(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, BIT(sds), BIT(sds));


	/* gating of ? */
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_DSDS_PAGE(31), 0x1, 0, 0, 0x1);

	rtpcs_931x_init_leq_dfe(ctrl, sds);


	/* media none behaviour */
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x12, 0x2740);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x0, 0x0);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x2, 0x2010);
	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 0xcd1);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 5, 0, 0x4);

	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2a), 0x12, 7, 6, 0x1);
	rtpcs_931x_sds_fiber_disable(ctrl, sds);

	regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_SPD_SEL], port, spd_ori);
	regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_FORCE_SPD_EN], port, spd_en_ori);

	rtpcs_931x_port_fib_unidir_set(ctrl, port, !!(unidir_sts & BIT_ULL(port)));

	rtpcs_sds_write(ctrl, even_sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x8, 0x294);

	switch (mode) {
	case RTPCS_SDS_MODE_10GBASER:
		/* from _dal_mango_construct_init_10gr */
		// rtpcs_sds_write_bits(ctrl, sds, 0x1f, 0xb, 1, 1, 1);

		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2a), 0x7, 15, 15, 0x1);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x3);

		// phy_rtl9310_10g_tx

		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0xf, 5, 0, 0x2);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0xd, 6, 6, 0x1);

		rtpcs_931x_sds_set_mode(ctrl, sds, mode);
		/* from _phy_rtl9310_10gMedia_set */
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x12, 0x27c0);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x0, 0xc000);
		rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2f), 0x2, 0x6010);
		break;

	case RTPCS_SDS_MODE_2500BASEX:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 0x14, 8, 8, 0x1);
		break;

	case RTPCS_SDS_MODE_1000BASEX:
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2a), 0x7, 15, 15, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x3);

		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x6), 0xd, 6, 6, 0x1);

		rtpcs_931x_sds_set_mode(ctrl, sds, mode);

		/* from _dal_mango_construct_init_fiber1g */
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x3), 0x13, 15, 14, 0x0);

		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x2), 0x0, 12, 12, 0x1);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x2), 0x0, 6, 6, 0x1);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x2), 0x0, 13, 13, 0x0);

		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x0), 0x4, 2, 2, 0x1);

		/* gating of ? */
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1f), 0x1, 0, 0, 0x0);
		break;
	default:
		return -EINVAL;
	}

	rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 0xc30);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 9, 0, 0x30);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2a), 0x12, 7, 6, 0x3);

	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x1);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x3);

	regmap_write_bits(ctrl->map, RTL931X_ISR_SERDES_RXIDLE, BIT(sds - 2), BIT(sds - 2));
	rtpcs_931x_sds_reset(ctrl, sds);
	regmap_write(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, ps_sds_sts);

	regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_FORCE_SPD_EN], port, 1);
	mdelay(50);
	regmap_fields_write(ctrl->rm_fields[RTL931X_SMI_FORCE_SPD_EN], port, spd_en_ori);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_DSDS_PAGE(31), 1, 0, 0, 0x0);

	return 0;
}

static inline void rtpcs_931x_sds_patch(struct rtpcs_ctrl *ctrl,
				   int sds,
				   sds_config *config,
				   size_t patch_len)
{
	while(patch_len) {
		rtpcs_sds_write(ctrl, sds, config->page, config->reg, config->data);
		config++;
		patch_len--;
	}
}

static int rtpcs_931x_sds_init(struct rtpcs_ctrl *ctrl, int sds, int chiptype)
{
	unsigned int val;
	regmap_write(ctrl->map, RTL931X_SERDES_BC_CTRL, 0x1f);
	regmap_write(ctrl->map, RTL931X_FRC_RXDV_H, 0xffffffff);
	regmap_write(ctrl->map, RTL931X_FRC_RXDV_L, 0xffffffff);

	// begin _dal_mango_construct_10g_sds_ana_patch
	if (chiptype) {
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_common_type1,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_common_type1));
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_10p3125g_type1,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_10p3125g_type1));
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_10p3125g_cmu_type1,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_10p3125g_cmu_type1));
		// patch ana_5g_type1
		// patch ana_5g_cmu_type1
		// patch ana_3p125g_type1
		// patch ana_3p125g_cmu_type1
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_2p5g_type1,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_2p5g_type1));
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_1p25g_type1,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_1p25g_type1));
	} else {
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_common,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_common));
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_10p3125g,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_10p3125g));
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_10p3125g_cmu,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_10p3125g_cmu));
		// patch 5g
		// patch 5g_cmu
		// patch 3p125g
		// patch 3p125g_cmu
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_2p5g,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_2p5g));
		rtpcs_931x_sds_patch(ctrl,
					sds,
					rtpcs_931x_sds_cfg_ana_1p25g,
					ARRAY_SIZE(rtpcs_931x_sds_cfg_ana_1p25g));
		val = 0xa0000;
		regmap_write(ctrl->map, RTL93XX_CHIP_INFO, val);
		regmap_read(ctrl->map, RTL93XX_CHIP_INFO, &val);
		if (val & BIT(28)) {
			rtpcs_931x_sds_patch(ctrl,
						sds,
						rtpcs_931x_sds_cfg_ana2,
						ARRAY_SIZE(rtpcs_931x_sds_cfg_ana2));
		}
		regmap_write(ctrl->map, RTL93XX_CHIP_INFO, 0);
		// Lots of unexplained specific serdes register writes for chiptype 0.
		// are handled here separately.

		switch(sds) {
		case 2:
			rtpcs_sds_write_bits(ctrl, 2, RTPCS_931X_ASDS_PAGE(0x2f), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 2, RTPCS_931X_ASDS_PAGE(0x2d), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 2, RTPCS_931X_ASDS_PAGE(0x2b), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 2, RTPCS_931X_ASDS_PAGE(0x25), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 2, RTPCS_931X_ASDS_PAGE(0x27), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 2, RTPCS_931X_ASDS_PAGE(0x29), 0x6, 15, 0, 0x5826);
			break;
		case 6:
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x2f), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x2f), 0x5, 15, 0, 0x3FD7);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x2d), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x2d), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x2b), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x2b), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x25), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x25), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x27), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x27), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x29), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 6, RTPCS_931X_ASDS_PAGE(0x29), 0x5, 15, 0, 0x27D7);
			break;
		case 10:
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x2f), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x2f), 0x5, 15, 0, 0x3FD7);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x2d), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x2d), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x2b), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x2b), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x25), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x25), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x27), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x27), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x29), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 10, RTPCS_931X_ASDS_PAGE(0x29), 0x5, 15, 0, 0x27D7);
			break;
		case 14:
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x2f), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x2f), 0x5, 15, 0, 0x3FD7);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x2d), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x2d), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x2b), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x2b), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x25), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x25), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x27), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x27), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x29), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 14, RTPCS_931X_ASDS_PAGE(0x29), 0x5, 15, 0, 0x27D7);
			break;
		case 18:
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x2f), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x2f), 0x5, 15, 0, 0x3FD7);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x2d), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x2d), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x2b), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x2b), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x25), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x25), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x27), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x27), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x29), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 18, RTPCS_931X_ASDS_PAGE(0x29), 0x5, 15, 0, 0x27D7);
			break;
		case 22:
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x2f), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x2f), 0x5, 15, 0, 0x3FD7);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x2d), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x2d), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x2b), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x2b), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x25), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x25), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x27), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x27), 0x5, 15, 0, 0x27D7);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x29), 0x6, 15, 0, 0x5826);
			rtpcs_sds_write_bits(ctrl, 22, RTPCS_931X_ASDS_PAGE(0x29), 0x5, 15, 0, 0x27D7);
			break;
		default:
			break;
		}
	}
	// end _dal_mango_construct_10g_sds_ana_patch
	return 0;
}


static int rtpcs_931x_setup_pcs_serdes(struct phylink_pcs *pcs, int sds, int port,
				   phy_interface_t mode)
{
	struct rtpcs_link *link = rtpcs_phylink_pcs_to_link(pcs);
	struct rtpcs_ctrl *ctrl = link->ctrl;


	u32 board_sds_tx_type1[] = {
		0x01c3, 0x01c3, 0x01c3, 0x01a3, 0x01a3, 0x01a3,
		0x0143, 0x0143, 0x0143, 0x0143, 0x0163, 0x0163,
	};
	u32 board_sds_tx[] = {
		0x1a00, 0x1a00, 0x0200, 0x0200, 0x0200, 0x0200,
		0x01a3, 0x01a3, 0x01a3, 0x01a3, 0x01e3, 0x01e3
	};
	u32 board_sds_tx2[] = {
		0x0dc0, 0x01c0, 0x0200, 0x0180, 0x0160, 0x0123,
		0x0123, 0x0163, 0x01a3, 0x01a0, 0x01c3, 0x09c3,
	};
	u32 ori, model_info, val;
	// enum rtpcs_sds_mode sds_mode;
	int chiptype = 0;
	u32 even_sds;

	if (sds < 0 || sds > 13)
		return -EINVAL;

	/*
	 * TODO: USXGMII is currently the swiss army knife to declare 10G
	 * multi port PHYs. Real devices use other modes instead. Especially
	 *
	 * - RTL8224 is driven in 10G_QXGMII
	 * - RTL8218D/E are driven in (Realtek proprietary) XSGMII (10G SGMII)
	 *
	 * For now disable all USXGMII SerDes handling and rely on U-Boot setup.
	 */
	// if (mode == PHY_INTERFACE_MODE_USXGMII)
		// return 0;

	even_sds = sds & ~1;

	pr_info("%s: set sds %d to mode %d\n", __func__, sds, mode);
	val = rtpcs_sds_read_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x1f), 0x9, 11, 6);

	pr_info("%s: fibermode %08X stored mode 0x%x", __func__,
			rtpcs_sds_read(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x1f), 0x9), val);
	pr_info("%s: SGMII mode %08X in 0x24 0x9", __func__,
			rtpcs_sds_read(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x24), 0x9));
	pr_info("%s: CMU mode %08X stored even SDS %d", __func__,
			rtpcs_sds_read(ctrl, sds & ~1, RTPCS_931X_ASDS_PAGE(0x20), 0x12), sds & ~1);
	pr_info("%s: serdes_mode_ctrl %08X", __func__,  RTL931X_SERDES_MODE_CTRL + 4 * (sds >> 2));
	pr_info("%s CMU page 0x24 0x7 %08x\n", __func__, rtpcs_sds_read(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x24), 0x7));
	pr_info("%s CMU page 0x26 0x7 %08x\n", __func__, rtpcs_sds_read(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x26), 0x7));
	pr_info("%s CMU page 0x28 0x7 %08x\n", __func__, rtpcs_sds_read(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x28), 0x7));
	pr_info("%s XSG page 0x0 0xe %08x\n", __func__, rtpcs_sds_read(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x0), 0xe));
	pr_info("%s XSG2 page 0x0 0xe %08x\n", __func__, rtpcs_sds_read(ctrl, sds, RTPCS_931X_XSGMSDS1_PAGE(0x0), 0xe));

	regmap_read(ctrl->map, RTL93XX_MODEL_NAME_INFO, &model_info);
	if ((model_info >> 4) & 0x1) {
		pr_info("detected chiptype 1\n");
		chiptype = 1;
	} else {
		pr_info("detected chiptype 0\n");
	}

	pr_info("%s: 2.5gbit %08X", __func__,
	        rtpcs_sds_read(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1), 0x14));
	rtpcs_931x_sds_init(ctrl, sds, chiptype);

	// Begin _dal_mango_construct_sdsMode_set

	regmap_read(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, &ori);
	pr_info("%s: RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR 0x%08X\n", __func__, ori);
	val = ori | (1 << sds);
	regmap_write(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, val);

	// band = rtpcs_931x_sds_get_cmu_band(ctrl, sds, link->sds_mode);

	switch (mode) {
	case PHY_INTERFACE_MODE_NA:
		goto out;

	case PHY_INTERFACE_MODE_USXGMII: /* MII_USXGMII_10GSXGMII/10GDXGMII/10GQXGMII: */
		/* TODO: implement that the other USXGMII submodes are supported too */
		link->sds_mode = RTPCS_SDS_MODE_USXGMII_10GSXGMII;
		rtpcs_931x_sds_config_usxgmii(ctrl, sds, chiptype);
		break;

	case PHY_INTERFACE_MODE_10GBASER:
		link->sds_mode = RTPCS_SDS_MODE_10GBASER;
		rtpcs_931x_sds_config_fiber(ctrl, sds, port, link->sds_mode);
		break;

	case PHY_INTERFACE_MODE_2500BASEX:
		link->sds_mode = RTPCS_SDS_MODE_2500BASEX;
		rtpcs_931x_sds_config_fiber(ctrl, sds, port, link->sds_mode);
		break;

	case PHY_INTERFACE_MODE_1000BASEX:
		link->sds_mode = RTPCS_SDS_MODE_1000BASEX;
		rtpcs_931x_sds_config_fiber(ctrl, sds, port, link->sds_mode);
		break;

	case PHY_INTERFACE_MODE_SGMII:
		link->sds_mode = RTPCS_SDS_MODE_SGMII;

		/* gating of ? */
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1f), 0x1, 0, 0, 0x1);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x24), 0x9, 15, 15, 0x0);
		rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_XSGMSDS_PAGE(0x1f), 0x1, 0, 0, 0x0);

		rtpcs_931x_sds_set_cmu_band(ctrl, sds, true, 62,
					    link->sds_mode);

		break;

	case PHY_INTERFACE_MODE_QSGMII:
	default:
		pr_info("%s: PHY mode %s not supported by SerDes %d\n",
		        __func__, phy_modes(mode), sds);
		return -ENOTSUPP;
	}

	rtpcs_931x_sds_set_cmu_type(ctrl, sds, link->sds_mode, chiptype);

	if (sds >= 2 && sds <= 13) {
		if (chiptype)
			rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1, board_sds_tx_type1[sds - 2]);
		else {
			val = 0xa0000;
			regmap_write(ctrl->map, RTL93XX_CHIP_INFO, val);
			regmap_read(ctrl->map, RTL93XX_CHIP_INFO, &val);
			if (val & BIT(28)) /* consider 9311 etc. RTL9313_CHIP_ID == HWP_CHIP_ID(unit)) */
			{
				rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2e), 0x1, board_sds_tx2[sds - 2]);
			} else {
				rtpcs_sds_write(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x2E), 0x1, board_sds_tx[sds - 2]);
			}
			val = 0;
			regmap_write(ctrl->map, RTL93XX_CHIP_INFO, val);
		}
	}

	val = ori & ~(1 << sds);
	regmap_write(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, val);
	/**
	 * TODO: This does not belong here..
	 * Where does this come from?
	 * */
	// rtpcs_931x_sds_set_polarity(ctrl, sds, ctrl->tx_pol_inv[sds],
				//    ctrl->rx_pol_inv[sds]);

	rtpcs_931x_sds_set_mode(ctrl, sds, link->sds_mode);

	// end: _dal_mango_construct_sdsMode_set
	mdelay(10);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x1);
	rtpcs_sds_write_bits(ctrl, sds, RTPCS_931X_ASDS_PAGE(0x20), 0x0, 11, 10, 0x3);
	mdelay(1000);

	// TODO: auto rx-calibration
	// Rely on auto rx-calibration


out:
	val = ori & ~BIT(sds);
	regmap_write(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, val);
	regmap_read(ctrl->map, RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR, &val);
	pr_debug("%s: RTL931X_PS_SERDES_OFF_MODE_CTRL_ADDR 0x%08X\n", __func__, val);

	return 0;
}

/* Common functions */

static void rtpcs_pcs_get_state(struct phylink_pcs *pcs, struct phylink_link_state *state)
{
	struct rtpcs_link *link = rtpcs_phylink_pcs_to_link(pcs);
	struct rtpcs_ctrl *ctrl = link->ctrl;
	int port = link->port;
	int linkup, speed;

	state->link = 0;
	state->speed = SPEED_UNKNOWN;
	state->duplex = DUPLEX_UNKNOWN;
	state->pause &= ~(MLO_PAUSE_RX | MLO_PAUSE_TX);

	/* Read MAC side link twice */
	for (int i = 0; i < 2; i++)
		linkup = rtpcs_regmap_read_bits(ctrl, ctrl->cfg->mac_link_sts, port, port);

	if (!linkup)
		return;

	state->link = 1;
	state->duplex = rtpcs_regmap_read_bits(ctrl, ctrl->cfg->mac_link_dup_sts, port, port);

	speed = rtpcs_regmap_read_bits(ctrl, ctrl->cfg->mac_link_spd_sts,
				       ctrl->cfg->mac_link_spd_bits * (port + 1) - 1,
				       ctrl->cfg->mac_link_spd_bits * port);
	switch (speed) {
	case RTPCS_SPEED_10:
		state->speed = SPEED_10;
		break;
	case RTPCS_SPEED_100:
		state->speed = SPEED_100;
		break;
	case RTPCS_SPEED_1000:
		state->speed = SPEED_1000;
		break;
	case RTPCS_SPEED_10000:
	case RTPCS_SPEED_10000_LEGACY:
		/*
		 * The legacy mode is ok so far with minor inconsistencies. On RTL838x this flag
		 * is either 500M or 2G. It might be that MAC_GLITE_STS register tells more. On
		 * RTL839x this is either 500M or 10G. More info might be in MAC_LINK_500M_STS.
		 * Without support for the 500M modes simply resolve to 10G.
		 */
		state->speed = SPEED_10000;
		break;
	case RTPCS_SPEED_2500:
		state->speed = SPEED_2500;
		break;
	case RTPCS_SPEED_5000:
		state->speed = SPEED_5000;
		break;
	default:
		dev_err(ctrl->dev, "unknown speed %d\n", speed);
	}

	if (rtpcs_regmap_read_bits(ctrl, ctrl->cfg->mac_rx_pause_sts, port, port))
		state->pause |= MLO_PAUSE_RX;
	if (rtpcs_regmap_read_bits(ctrl, ctrl->cfg->mac_tx_pause_sts, port, port))
		state->pause |= MLO_PAUSE_TX;
}

static void rtpcs_pcs_an_restart(struct phylink_pcs *pcs)
{
	struct rtpcs_link *link = rtpcs_phylink_pcs_to_link(pcs);
	struct rtpcs_ctrl *ctrl = link->ctrl;


	dev_warn(ctrl->dev, "an_restart() for port %d and sds %d not yet implemented\n",
		 link->port, link->sds);
}

static int rtpcs_pcs_config(struct phylink_pcs *pcs, unsigned int neg_mode,
			    phy_interface_t interface, const unsigned long *advertising,
			    bool permit_pause_to_mac)
{
	struct rtpcs_link *link = rtpcs_phylink_pcs_to_link(pcs);
	struct rtpcs_ctrl *ctrl = link->ctrl;
	int ret = 0;

	if (link->sds < 0)
		return 0;

	/*
	 * TODO: This (or copies of this) will be the central function for configuring the
	 * link between PHY and SerDes. As of now a lot of the code is scattered throughout
	 * all the other Realtek drivers. Maybe some day this will live up to the expectations.
	 */

	dev_warn(ctrl->dev, "pcs_config(%s) for port %d and sds %d not yet fully implemented\n",
		 phy_modes(interface), link->port, link->sds);

	mutex_lock(&ctrl->lock);

	if (ctrl->cfg->setup_serdes) {
		ret = ctrl->cfg->setup_serdes(ctrl, link->sds, interface);
		if (ret < 0)
			goto out;
	} else if (ctrl->cfg->setup_pcs_serdes) {
		ret = ctrl->cfg->setup_pcs_serdes(pcs, link->sds, link->port, interface);
		if (ret < 0)
			goto out;
	}

	if (ctrl->cfg->set_autoneg) {
		ret = ctrl->cfg->set_autoneg(ctrl, link->sds, neg_mode);
		if (ret < 0)
			goto out;
	}

out:
	mutex_unlock(&ctrl->lock);

	return ret;
}

struct phylink_pcs *rtpcs_create(struct device *dev, struct device_node *np, int port);
struct phylink_pcs *rtpcs_create(struct device *dev, struct device_node *np, int port)
{
	struct platform_device *pdev;
	struct device_node *pcs_np;
	struct rtpcs_ctrl *ctrl;
	struct rtpcs_link *link;
	int sds;

	/*
	 * RTL838x devices have a built-in octa port RTL8218B PHY that is not attached via
	 * a SerDes. Allow to be called with an empty SerDes device node. In this case lookup
	 * the parent/driver node directly.
	 */
	if (np) {
		if (!of_device_is_available(np))
			return ERR_PTR(-ENODEV);

		if (of_property_read_u32(np, "reg", &sds))
			return ERR_PTR(-EINVAL);

		pcs_np = of_get_parent(np);
	} else {
		pcs_np = of_find_compatible_node(NULL, NULL, "realtek,otto-pcs");
		sds = -1;
	}

	if (!pcs_np)
		return ERR_PTR(-ENODEV);

	if (!of_device_is_available(pcs_np)) {
		of_node_put(pcs_np);
		return ERR_PTR(-ENODEV);
	}

	pdev = of_find_device_by_node(pcs_np);
	of_node_put(pcs_np);
	if (!pdev)
		return ERR_PTR(-EPROBE_DEFER);

	ctrl = platform_get_drvdata(pdev);
	if (!ctrl) {
		put_device(&pdev->dev);
		return ERR_PTR(-EPROBE_DEFER);
	}

	if (port < 0 || port > ctrl->cfg->cpu_port)
		return ERR_PTR(-EINVAL);

	if (sds !=-1 && rtpcs_sds_read(ctrl, sds, 0 , 0) < 0)
		return ERR_PTR(-EINVAL);

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link) {
		put_device(&pdev->dev);
		return ERR_PTR(-ENOMEM);
	}

	device_link_add(dev, ctrl->dev, DL_FLAG_AUTOREMOVE_CONSUMER);

	link->ctrl = ctrl;
	link->port = port;
	link->sds = sds;
	link->pcs.ops = ctrl->cfg->pcs_ops;
	link->pcs.neg_mode = true;

	ctrl->link[port] = link;

	dev_dbg(ctrl->dev, "phylink_pcs created, port %d, sds %d\n", port, sds);

	return &link->pcs;
}
EXPORT_SYMBOL(rtpcs_create);

static struct mii_bus *rtpcs_probe_serdes_bus(struct rtpcs_ctrl *ctrl)
{
	struct device_node *np;
	struct mii_bus *bus;
	int err;

	np = of_find_compatible_node(NULL, NULL, "realtek,otto-serdes-mdio");
	if (!np) {
		dev_err(ctrl->dev, "SerDes mdio bus not found in DT");
		return ERR_PTR(-ENODEV);
	}

	bus = of_mdio_find_bus(np);
	of_node_put(np);
	if (!bus) {
		dev_warn(ctrl->dev, "SerDes mdio bus not (yet) active");
		return ERR_PTR(-EPROBE_DEFER);
	}

	if (!of_device_is_available(np)) {
		dev_err(ctrl->dev, "SerDes mdio bus not usable");
		return ERR_PTR(-ENODEV);
	}

	ctrl->wq = create_singlethread_workqueue("pcs-rtl-otto");
	if (!ctrl->wq) {
		dev_err(ctrl->dev,
			"Error creating workqueue: %d\n",
			err);
		return ERR_PTR(-ENOMEM);
	}

	if (ctrl->cfg->init_link_check) {
		ctrl->cfg->init_link_check(ctrl);
	}

	return bus;
}

static int rtpcs_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct device_node *child;
	struct rtpcs_ctrl *ctrl;
	u32 sds;
	int ret;

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	mutex_init(&ctrl->lock);

	ctrl->dev = dev;
	ctrl->cfg = (const struct rtpcs_config *)device_get_match_data(ctrl->dev);
	ctrl->map = syscon_node_to_regmap(np->parent);
	if (IS_ERR(ctrl->map))
		return PTR_ERR(ctrl->map);

	if (ctrl->cfg->reg_fields) {
		#if 0
		// ctrl->rm_fields = devm_kzalloc(dev, ctrl->cfg->num_reg_fields * sizeof(struct regmap_field *), GFP_KERNEL);
		pr_err("[hh CC] %s: ctrl->rm_fields: %px\n", __func__, ctrl->rm_fields);
		for (int i = 0; i < ctrl->cfg->num_reg_fields; i++) {
			ctrl->rm_fields[i] = devm_regmap_field_alloc(dev, ctrl->map, ctrl->cfg->reg_fields[i]);
			if (IS_ERR(ctrl->rm_fields[i]))
				return PTR_ERR(ctrl->rm_fields[i]);
			pr_err("[hh CC] %s: ctrl->rm_fields[i:%d]: %px\n", __func__, i, ctrl->rm_fields[i]);
		}
		#else
		ctrl->rm_fields = devm_kzalloc(dev, ctrl->cfg->num_reg_fields * sizeof(struct regmap_field *), GFP_KERNEL);
		if (!ctrl->rm_fields)
			return -ENOMEM;
		if (devm_regmap_field_bulk_alloc(dev,
						 ctrl->map,
						 ctrl->rm_fields,
						 ctrl->cfg->reg_fields,
						 ctrl->cfg->num_reg_fields)) {
			return -ENOMEM;
		}
		#endif

	}


	// ctrl->map->cache_bypass = true;
	// ctrl->map->cache_only = false;

	ctrl->bus = rtpcs_probe_serdes_bus(ctrl);
	if (IS_ERR(ctrl->bus))
		return PTR_ERR(ctrl->bus);

	for_each_child_of_node(dev->of_node, child) {
		ret = of_property_read_u32(child, "reg", &sds);
		if (ret)
			return ret;
		if (sds >= RTPCS_SDS_CNT)
			return -EINVAL;

		ctrl->rx_pol_inv[sds] = of_property_read_bool(child, "realtek,pnswap-rx");
		ctrl->tx_pol_inv[sds] = of_property_read_bool(child, "realtek,pnswap-tx");
	}

	/*
	 * rtpcs_create() relies on that fact that data is attached to the platform device to
	 * determine if the driver is ready. Do this after everything is initialized properly.
	 */
	platform_set_drvdata(pdev, ctrl);

	dev_warn(dev, "Realtek PCS driver initialized\n");

	return 0;
}

static int rtpcs_93xx_set_autoneg(struct rtpcs_ctrl *ctrl, int sds,
				  unsigned int neg_mode)
{
	u16 bmcr = neg_mode == PHYLINK_PCS_NEG_INBAND_ENABLED ? BMCR_ANENABLE : 0;

	return rtpcs_sds_modify(ctrl, sds, 2, MII_BMCR, BMCR_ANENABLE, bmcr);
}

static const struct phylink_pcs_ops rtpcs_838x_pcs_ops = {
	.pcs_an_restart		= rtpcs_pcs_an_restart,
	.pcs_config		= rtpcs_pcs_config,
	.pcs_get_state		= rtpcs_pcs_get_state,
};

static const struct rtpcs_config rtpcs_838x_cfg = {
	.cpu_port		= RTPCS_838X_CPU_PORT,
	.mac_link_dup_sts	= RTPCS_838X_MAC_LINK_DUP_STS,
	.mac_link_spd_sts	= RTPCS_838X_MAC_LINK_SPD_STS,
	.mac_link_spd_bits	= RTPCS_83XX_MAC_LINK_SPD_BITS,
	.mac_link_sts		= RTPCS_838X_MAC_LINK_STS,
	.mac_rx_pause_sts	= RTPCS_838X_MAC_RX_PAUSE_STS,
	.mac_tx_pause_sts	= RTPCS_838X_MAC_TX_PAUSE_STS,
	.pcs_ops		= &rtpcs_838x_pcs_ops,
};

static const struct phylink_pcs_ops rtpcs_839x_pcs_ops = {
	.pcs_an_restart		= rtpcs_pcs_an_restart,
	.pcs_config		= rtpcs_pcs_config,
	.pcs_get_state		= rtpcs_pcs_get_state,
};

static const struct rtpcs_config rtpcs_839x_cfg = {
	.cpu_port		= RTPCS_839X_CPU_PORT,
	.mac_link_dup_sts	= RTPCS_839X_MAC_LINK_DUP_STS,
	.mac_link_spd_sts	= RTPCS_839X_MAC_LINK_SPD_STS,
	.mac_link_spd_bits	= RTPCS_83XX_MAC_LINK_SPD_BITS,
	.mac_link_sts		= RTPCS_839X_MAC_LINK_STS,
	.mac_rx_pause_sts	= RTPCS_839X_MAC_RX_PAUSE_STS,
	.mac_tx_pause_sts	= RTPCS_839X_MAC_TX_PAUSE_STS,
	.pcs_ops		= &rtpcs_839x_pcs_ops,
};

static const struct phylink_pcs_ops rtpcs_930x_pcs_ops = {
	.pcs_an_restart		= rtpcs_pcs_an_restart,
	.pcs_config		= rtpcs_pcs_config,
	.pcs_get_state		= rtpcs_pcs_get_state,
};

static const struct rtpcs_config rtpcs_930x_cfg = {
	.cpu_port		= RTPCS_930X_CPU_PORT,
	.mac_link_dup_sts	= RTPCS_930X_MAC_LINK_DUP_STS,
	.mac_link_spd_sts	= RTPCS_930X_MAC_LINK_SPD_STS,
	.mac_link_spd_bits	= RTPCS_93XX_MAC_LINK_SPD_BITS,
	.mac_link_sts		= RTPCS_930X_MAC_LINK_STS,
	.mac_rx_pause_sts	= RTPCS_930X_MAC_RX_PAUSE_STS,
	.mac_tx_pause_sts	= RTPCS_930X_MAC_TX_PAUSE_STS,
	.pcs_ops		= &rtpcs_930x_pcs_ops,
	.set_autoneg		= rtpcs_93xx_set_autoneg,
	.setup_serdes		= rtpcs_930x_setup_serdes,
};

static const struct phylink_pcs_ops rtpcs_931x_pcs_ops = {
	.pcs_an_restart		= rtpcs_pcs_an_restart,
	.pcs_config		= rtpcs_pcs_config,
	.pcs_get_state		= rtpcs_pcs_get_state,
};

static const struct rtpcs_config rtpcs_931x_cfg = {
	.cpu_port		= RTPCS_931X_CPU_PORT,
	.mac_link_dup_sts	= RTPCS_931X_MAC_LINK_DUP_STS,
	.mac_link_spd_sts	= RTPCS_931X_MAC_LINK_SPD_STS,
	.mac_link_spd_bits	= RTPCS_93XX_MAC_LINK_SPD_BITS,
	.mac_link_sts		= RTPCS_931X_MAC_LINK_STS,
	.mac_rx_pause_sts	= RTPCS_931X_MAC_RX_PAUSE_STS,
	.mac_tx_pause_sts	= RTPCS_931X_MAC_TX_PAUSE_STS,
	.pcs_ops		= &rtpcs_931x_pcs_ops,
	.set_autoneg		= rtpcs_93xx_set_autoneg,
	.setup_pcs_serdes	= rtpcs_931x_setup_pcs_serdes,
	.init_link_check	= rtpcs_931x_init_link_check,
	.reg_fields 		= rtpcs_931x_reg_fields,
	.num_reg_fields		= RTL931X_MAX_FIELDS,
};

static const struct of_device_id rtpcs_of_match[] = {
	{
		.compatible = "realtek,rtl8380-pcs",
		.data = &rtpcs_838x_cfg,
	},
	{
		.compatible = "realtek,rtl8392-pcs",
		.data = &rtpcs_839x_cfg,
	},
	{
		.compatible = "realtek,rtl9301-pcs",
		.data = &rtpcs_930x_cfg,
	},
	{
		.compatible = "realtek,rtl9311-pcs",
		.data = &rtpcs_931x_cfg,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rtpcs_of_match);

static struct platform_driver rtpcs_driver = {
	.driver = {
		.name = "realtek-otto-pcs",
		.of_match_table = rtpcs_of_match
	},
	.probe = rtpcs_probe,
};
module_platform_driver(rtpcs_driver);

MODULE_AUTHOR("Markus Stockhausen <markus.stockhausen@gmx.de>");
MODULE_DESCRIPTION("Realtek Otto SerDes PCS driver");
MODULE_LICENSE("GPL v2");
