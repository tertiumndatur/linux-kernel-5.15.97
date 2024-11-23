#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/backlight.h>
#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_connector.h>
#include <video/mipi_display.h>
#include <linux/printk.h>

struct umo_p076md_t {
    struct drm_panel panel;
    struct mipi_dsi_device *dsi;
    struct regulator *vcc_mipi;
    struct backlight_device *backlight;
};


static int umo_p076md_t_unprepare(struct drm_panel *panel)
{
    struct umo_p076md_t *ctx = container_of(panel, struct umo_p076md_t, panel);

    mipi_dsi_dcs_write(ctx->dsi, 0x28, NULL, 0);
    msleep(50);
    mipi_dsi_dcs_write(ctx->dsi, 0x10, NULL, 0);
    msleep(50);

    msleep(10);
    return 0;
}

static int umo_p076md_t_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
    struct drm_display_mode *mode;

    printk(KERN_INFO "umo_p076md_t: get_modes called\n");

    connector->display_info.width_mm = 94;
    connector->display_info.height_mm = 150;
    drm_connector_set_panel_orientation(connector, 2);
    connector->display_info.panel_orientation = 2;

    mode = drm_mode_create(connector->dev);
    if (!mode) {
        printk(KERN_ERR "umo_p076md_t: Failed to create display mode\n");
        return -ENOMEM;
    }


    mode->clock = 74250;
    mode->hdisplay = 800;
    mode->hsync_start = 800 + 76;
    mode->hsync_end = 800 + 76 + 18;
    mode->htotal = 800 + 76 + 18 + 76;
    mode->vdisplay = 1280;
    mode->vsync_start = 1280 + 4;
    mode->vsync_end = 1280 + 4 + 2;
    mode->vtotal = 1280 + 4 + 2 + 4;

    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

    drm_mode_set_name(mode);
    drm_mode_probed_add(connector, mode);

    printk(KERN_INFO "umo_p076md_t: Added display mode: %dx%d@59.34Hz\n",
           mode->hdisplay, mode->vdisplay);

    return 1;
}


static const struct drm_panel_funcs umo_p076md_t_funcs = {
    .unprepare = umo_p076md_t_unprepare,
    .get_modes = umo_p076md_t_get_modes,
};

static int umo_p076md_t_probe(struct mipi_dsi_device *dsi)
{
    struct device *dev = &dsi->dev;
    struct umo_p076md_t *ctx;
    int ret;

    printk(KERN_INFO "umo_p076md_t: Probing panel...\n");

    ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
    if (!ctx) {
        printk(KERN_ERR "umo_p076md_t: Failed to allocate memory for panel context\n");
        return -ENOMEM;
    }

    ctx->dsi = dsi;
    mipi_dsi_set_drvdata(dsi, ctx);

    ctx->vcc_mipi = devm_regulator_get(dev, "vcc_mipi");
    if (IS_ERR(ctx->vcc_mipi)) {
        ret = PTR_ERR(ctx->vcc_mipi);
        dev_err(dev, "Failed to get vcc_mipi regulator: %d\n", ret);
        return ret;
    }

    ret = regulator_enable(ctx->vcc_mipi);
    if (ret) {
        dev_err(dev, "Failed to enable vcc_mipi regulator: %d\n", ret);
        return ret;
    }

    msleep(200);

    drm_panel_init(&ctx->panel, dev, &umo_p076md_t_funcs, DRM_MODE_CONNECTOR_DSI);
    drm_panel_add(&ctx->panel);

    dsi->lanes = 4;
    dsi->format = MIPI_DSI_FMT_RGB888;
    dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM;

    printk(KERN_INFO "umo_p076md_t: Configuring MIPI-DSI: lanes=%d, format=0x%x, mode_flags=0x%x\n",
           dsi->lanes, dsi->format, dsi->mode_flags);

    ret = mipi_dsi_attach(dsi);
    if (ret) {
        printk(KERN_ERR "umo_p076md_t: Failed to attach MIPI-DSI: %d\n", ret);
        drm_panel_remove(&ctx->panel);
        return ret;
    }

    printk(KERN_INFO "umo_p076md_t: Panel probe completed successfully\n");
    return 0;
}


static int umo_p076md_t_remove(struct mipi_dsi_device *dsi)
{
    struct umo_p076md_t *ctx = mipi_dsi_get_drvdata(dsi);

    drm_panel_remove(&ctx->panel);
    mipi_dsi_detach(dsi);

    return 0;
}

static const struct of_device_id umo_p076md_t_of_match[] = {
    { .compatible = "urt,umo-p076md-t" },
    { }
};
MODULE_DEVICE_TABLE(of, umo_p076md_t_of_match);

static struct mipi_dsi_driver umo_p076md_t_driver = {
    .driver = {
        .name = "umo_p076md_t",
        .of_match_table = umo_p076md_t_of_match,
    },
    .probe = umo_p076md_t_probe,
    .remove = umo_p076md_t_remove,
};
module_mipi_dsi_driver(umo_p076md_t_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Driver for UMO-P076MD-T MIPI-DSI Panel");
MODULE_LICENSE("GPL");


