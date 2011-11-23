
#ifndef __GST_SWCSC_H__
#define __GST_SWCSC_H__

#include <pthread.h>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIC6xColorspace objects */
#define GST_TYPE_SWCSC \
  (gst_swcsc_get_type())
#define GST_SWCSC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SWCSC,GstSwcsc))
#define GST_SWCSC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SWCSC,GstSwcscClass))
#define GST_IS_SWCSC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SWCSC))
#define GST_IS_SWCSC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SWCSC))

typedef struct _GstSwcsc      GstSwcsc;
typedef struct _GstSwcscClass GstSwcscClass;

/* _GstTISwcsc object */
struct _GstSwcsc
{
  /* gStreamer infrastructure */
  GstBaseTransform  element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  gint crop_left, crop_top, crop_width, crop_height;
  gint width, height;

};

/* _GstTISwcscClass object */
struct _GstSwcscClass 
{
  GstBaseTransformClass parent_class;
};

/* External function enclarations */
GType gst_swcsc_get_type(void);

G_END_DECLS

#endif /* __GST_SWCSC_H__ */

