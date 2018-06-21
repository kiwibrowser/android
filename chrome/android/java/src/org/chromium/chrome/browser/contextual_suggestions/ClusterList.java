// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.chrome.browser.ntp.cards.ChildNode;
import org.chromium.chrome.browser.ntp.cards.InnerNode;

import java.util.List;

/**
 * A node in a tree containing a list of {@link ContextualSuggestionsCluster}s.
 */
class ClusterList extends InnerNode {
    /**
     * Replaces the list of clusters under this node with a new list. Any previous clusters will be
     * destroyed.
     *
     * @param clusters The new list of clusters for this node.
     */
    public void setClusters(List<ContextualSuggestionsCluster> clusters) {
        destroyClusters();
        removeChildren();
        for (ContextualSuggestionsCluster cluster : clusters) {
            addChildren(cluster);
        }
    }

    /**
     * Destroys all clusters under this node.
     */
    public void destroy() {
        destroyClusters();
    }

    private void destroyClusters() {
        for (ChildNode c : getChildren()) {
            ((ContextualSuggestionsCluster) c).destroy();
        }
    }
}
