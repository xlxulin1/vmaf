/**
 *
 *  Copyright 2016-2020 Netflix, Inc.
 *
 *     Licensed under the BSD+Patent License (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         https://opensource.org/licenses/BSDplusPatent
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#include <errno.h>
#include <stddef.h>

#include "feature_collector.h"
#include "feature_extractor.h"

#include "mem.h"
#include "ssim.h"
#include "picture_copy.h"

typedef struct SsimState {
    size_t float_stride;
    float *ref;
    float *dist;
    bool enable_lcs;
} SsimState;

static const VmafOption options[] = {
    {
        .name = "enable_lcs",
        .help = "enable luminance, contrast and structure intermediate output",
        .offset = offsetof(SsimState, enable_lcs),
        .type = VMAF_OPT_TYPE_BOOL,
        .default_val.b = false,
    },
    { NULL }
};

static int init(VmafFeatureExtractor *fex, enum VmafPixelFormat pix_fmt,
                unsigned bpc, unsigned w, unsigned h)
{
    SsimState *s = fex->priv;
    s->float_stride = ALIGN_CEIL(w * sizeof(float));
    s->ref = aligned_malloc(s->float_stride * h, 32);
    if (!s->ref) goto fail;
    s->dist = aligned_malloc(s->float_stride * h, 32);
    if (!s->dist) goto free_ref;

    return 0;

free_ref:
    free(s->ref);
fail:
    return -ENOMEM;
}

static int extract(VmafFeatureExtractor *fex,
                   VmafPicture *ref_pic, VmafPicture *dist_pic,
                   unsigned index, VmafFeatureCollector *feature_collector)
{
    SsimState *s = fex->priv;
    int err = 0;

    picture_copy(s->ref, s->float_stride, ref_pic, 0, ref_pic->bpc);
    picture_copy(s->dist, s->float_stride, dist_pic, 0, dist_pic->bpc);

    double score, l_score, c_score, s_score;
    err = compute_ssim(s->ref, s->dist, ref_pic->w[0], ref_pic->h[0], s->float_stride,
                       s->float_stride, &score, &l_score, &c_score, &s_score);
    if (err) return err;
    err = vmaf_feature_collector_append(feature_collector, "float_ssim",
                                        score, index);
    if (err) return err;
    if (s->enable_lcs) {
        err = vmaf_feature_collector_append(feature_collector, "float_ssim_l",
                                            l_score, index);
        if (err) return err;
        err = vmaf_feature_collector_append(feature_collector, "float_ssim_c",
                                            c_score, index);
        if (err) return err;
        err = vmaf_feature_collector_append(feature_collector, "float_ssim_s",
                                            s_score, index);
        if (err) return err;
    }
    return 0;
}

static int close(VmafFeatureExtractor *fex)
{
    SsimState *s = fex->priv;
    if (s->ref) aligned_free(s->ref);
    if (s->dist) aligned_free(s->dist);
    return 0;
}

static const char *provided_features[] = {
    "float_ssim",
    NULL
};

VmafFeatureExtractor vmaf_fex_float_ssim = {
    .name = "float_ssim",
    .init = init,
    .extract = extract,
    .options = options,
    .close = close,
    .priv_size = sizeof(SsimState),
    .provided_features = provided_features,
};
