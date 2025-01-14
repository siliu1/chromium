// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_POSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_POSE_H_

#include <utility>

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRRigidTransform;
class XRSession;
class XRView;

class XRViewerPose final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRViewerPose(XRSession*, std::unique_ptr<TransformationMatrix>);

  XRRigidTransform* transform() const { return transform_; }
  const HeapVector<Member<XRView>>& views() const { return views_; }

  void Trace(blink::Visitor*) override;

 private:
  const Member<XRSession> session_;
  Member<XRRigidTransform> transform_;
  HeapVector<Member<XRView>> views_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_POSE_H_
