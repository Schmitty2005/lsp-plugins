/*
 * KVTStorage.cpp
 *
 *  Created on: 30 мая 2019 г.
 *      Author: sadko
 */

#include <core/KVTStorage.h>
#include <core/stdlib/string.h>

namespace lsp
{
    KVTListener::KVTListener()
    {
    }

    KVTListener::~KVTListener()
    {
    }

    void KVTListener::attached(KVTStorage *storage)
    {
    }

    void KVTListener::detached(KVTStorage *storage)
    {
    }

    void KVTListener::created(const char *id, const kvt_param_t *param)
    {
    }

    void KVTListener::rejected(const char *id, const kvt_param_t *rej, const kvt_param_t *curr)
    {
    }

    void KVTListener::changed(const char *id, const kvt_param_t *oval, const kvt_param_t *nval)
    {
    }

    void KVTListener::removed(const char *id, const kvt_param_t *param)
    {
    }

    void KVTListener::access(const char *id, const kvt_param_t *param)
    {
    }

    void KVTListener::commit(const char *id, const kvt_param_t *param)
    {
    }

    void KVTListener::missed(const char *id)
    {
    }

    
    KVTStorage::KVTStorage(char separator)
    {
        cSeparator  = separator;

        sValid.next         = NULL;
        sValid.prev         = NULL;
        sDirty.next         = NULL;
        sDirty.prev         = NULL;
        sDirty.node         = NULL;
        sGarbage.next       = NULL;
        sGarbage.prev       = NULL;
        sGarbage.node       = NULL;
        pTrash              = NULL;
        pIterators          = NULL;
        nNodes              = 0;
        nValues             = 0;
        nModified           = 0;

        init_node(&sRoot, NULL, 0);
        ++sRoot.refs;
    }
    
    KVTStorage::~KVTStorage()
    {
        destroy();
    }

    void KVTStorage::destroy()
    {
        unbind_all();

        // Destroy trash
        while (pTrash != NULL)
        {
            kvt_gcparam_t *next = pTrash->next;
            destroy_parameter(pTrash);
            pTrash      = next;
        }

        // Destroy all iterators
        while (pIterators != NULL)
        {
            KVTIterator *next   = pIterators->pGcNext;
            delete pIterators;
            pIterators          = next;
        }

        // Destroy all nodes
        kvt_link_t *link = sValid.next;
        while (link != NULL)
        {
            kvt_link_t *next = link->next;
            destroy_node(link->node);
            link        = next;
        }
        link = sGarbage.next;
        while (link != NULL)
        {
            kvt_link_t *next = link->next;
            destroy_node(link->node);
            link        = next;
        }

        // Cleanup pointers
        sRoot.id            = NULL;
        sRoot.idlen         = 0;
        sRoot.parent        = NULL;
        sRoot.refs          = 0;
        sRoot.param         = NULL;
        sRoot.gc.next       = NULL;
        sRoot.gc.prev       = NULL;
        sRoot.gc.node       = NULL;
        sRoot.mod.next      = NULL;
        sRoot.mod.prev      = NULL;
        sRoot.mod.node      = NULL;
        if (sRoot.children != NULL)
        {
            ::free(sRoot.children);
            sRoot.children      = NULL;
        }
        sRoot.nchildren     = 0;
        sRoot.capacity      = 0;

        sValid.next         = NULL;
        sValid.prev         = NULL;
        sValid.node         = NULL;
        sDirty.next         = NULL;
        sDirty.prev         = NULL;
        sDirty.node         = NULL;
        sGarbage.next       = NULL;
        sGarbage.prev       = NULL;
        sGarbage.node       = NULL;
        pTrash              = NULL;
        pIterators          = NULL;

        nNodes              = 0;
        nValues             = 0;
        nModified           = 0;
    }

    status_t KVTStorage::clear()
    {
        return do_remove_branch("/", &sRoot);
    }

    void KVTStorage::init_node(kvt_node_t *node, const char *name, size_t len)
    {
        node->id            = (name != NULL) ? reinterpret_cast<char *>(&node[1]) : NULL;
        node->idlen         = len;
        node->parent        = NULL;
        node->refs          = 0;
        node->param         = NULL;
        node->modified      = false;
        node->gc.next       = NULL;
        node->gc.prev       = NULL;
        node->gc.node       = node;
        node->mod.next      = NULL;
        node->mod.prev      = NULL;
        node->mod.node      = node;
        node->children      = NULL;
        node->nchildren     = 0;
        node->capacity      = 0;

        // Copy name
        if (node->id != NULL)
        {
            ::memcpy(node->id, name, len);
            node->id[len]       = '\0';
        }
    }

    size_t KVTStorage::listeners() const
    {
        return vListeners.size();
    }

    KVTStorage::kvt_node_t *KVTStorage::allocate_node(const char *name, size_t len)
    {
        size_t to_alloc     = ALIGN_SIZE(sizeof(kvt_node_t) + len + 1, DEFAULT_ALIGN);
        kvt_node_t *node    = reinterpret_cast<kvt_node_t *>(::malloc(to_alloc));
        if (node != NULL)
        {
            init_node(node, name, len);
            link_list(&sGarbage, &node->gc);    // By default, add to garbage
        }

        return node;
    }

    void KVTStorage::link_list(kvt_link_t *root, kvt_link_t *item)
    {
        item->next          = root->next;
        item->prev          = root;
        if (root->next != NULL)
            root->next->prev    = item;
        root->next          = item;
    }

    void KVTStorage::unlink_list(kvt_link_t *item)
    {
        if (item->prev != NULL)
            item->prev->next    = item->next;
        if (item->next != NULL)
            item->next->prev    = item->prev;
        item->next      = NULL;
        item->prev      = NULL;
    }

    void KVTStorage::mark_dirty(kvt_node_t *node)
    {
        if (node->modified)
            return;

        // Link to dirty list
        link_list(&sDirty, &node->mod);
        node->modified      = true;
        ++nModified;
    }

    void KVTStorage::mark_clean(kvt_node_t *node)
    {
        if (!node->modified)
            return;

        // Unlink from dirty list
        unlink_list(&node->mod);
        node->modified      = false;
        --nModified;
    }

    KVTStorage::kvt_node_t *KVTStorage::reference_up(kvt_node_t *node)
    {
        if ((node->refs++) > 0)
            return node;

        // Move to valid
        unlink_list(&node->gc);
        link_list(&sValid, &node->gc);
        ++nNodes;

        return node;
    }

    KVTStorage::kvt_node_t *KVTStorage::reference_down(kvt_node_t *node)
    {
        kvt_node_t *x = node;

        do
        {
            // Decrement number of references
            if ((--x->refs) > 0)
                break;

            // Move to garbage
            unlink_list(&node->gc);
            link_list(&sGarbage, &node->gc);
            --nNodes;

            // Move to parent
            x = x->parent;
        } while (x != NULL);

        return node;
    }

    void KVTStorage::notify_created(const char *id, const kvt_param_t *param)
    {
        for (size_t i=0, n=vListeners.size(); i<n; ++i)
        {
            KVTListener *listener = vListeners.at(i);
            if (listener != NULL)
                listener->created(id, param);
        }
    }

    void KVTStorage::notify_rejected(const char *id, const kvt_param_t *rej, const kvt_param_t *curr)
    {
        for (size_t i=0, n=vListeners.size(); i<n; ++i)
        {
            KVTListener *listener = vListeners.at(i);
            if (listener != NULL)
                listener->rejected(id, rej, curr);
        }
    }

    void KVTStorage::notify_changed(const char *id, const kvt_param_t *oval, const kvt_param_t *nval)
    {
        for (size_t i=0, n=vListeners.size(); i<n; ++i)
        {
            KVTListener *listener = vListeners.at(i);
            if (listener != NULL)
                listener->changed(id, oval, nval);
        }
    }

    void KVTStorage::notify_removed(const char *id, const kvt_param_t *param)
    {
        for (size_t i=0, n=vListeners.size(); i<n; ++i)
        {
            KVTListener *listener = vListeners.at(i);
            if (listener != NULL)
                listener->removed(id, param);
        }
    }

    void KVTStorage::notify_access(const char *id, const kvt_param_t *param)
    {
        for (size_t i=0, n=vListeners.size(); i<n; ++i)
        {
            KVTListener *listener = vListeners.at(i);
            if (listener != NULL)
                listener->access(id, param);
        }
    }

    void KVTStorage::notify_commit(const char *id, const kvt_param_t *param)
    {
        for (size_t i=0, n=vListeners.size(); i<n; ++i)
        {
            KVTListener *listener = vListeners.at(i);
            if (listener != NULL)
                listener->commit(id, param);
        }
    }

    void KVTStorage::notify_missed(const char *id)
    {
        for (size_t i=0, n=vListeners.size(); i<n; ++i)
        {
            KVTListener *listener = vListeners.at(i);
            if (listener != NULL)
                listener->missed(id);
        }
    }

    void KVTStorage::destroy_parameter(kvt_gcparam_t *param)
    {
        // Destroy extra data
        if (param->type == KVT_STRING)
        {
            if (param->str != NULL)
                ::free(const_cast<char *>(param->str));
            param->u64      = 0;
        }
        else if (param->type == KVT_BLOB)
        {
            if (param->blob.ctype != NULL)
            {
                ::free(const_cast<char *>(param->blob.ctype));
                param->blob.ctype   = NULL;
            }
            if (param->blob.data != NULL)
            {
                ::free(const_cast<void *>(param->blob.data));
                param->blob.data    = NULL;
            }
            param->blob.size    = 0;
        }
        else
            param->u64      = 0;

        param->type         = KVT_ANY;
        ::free(param);
    }

    char *KVTStorage::build_path(char **path, size_t *capacity, const kvt_node_t *node)
    {
        // Estimate number of bytes required
        size_t bytes = 1;
        for (const kvt_node_t *n = node; n != &sRoot; n = n->parent)
            bytes   += n->idlen + 1;

        char *dst   = *path;
        size_t rcap = (bytes + 0x1f) & (~size_t(0x1f));
        if (rcap > *capacity)
        {
            dst = reinterpret_cast<char *>(::realloc(*path, rcap));
            if (dst == NULL)
                return NULL;
            *capacity       = rcap;
            *path           = dst;
        }

        dst         = &dst[bytes];
        *(--dst)    = '\0';
        for (const kvt_node_t *n = node; n != &sRoot; n = n->parent)
        {
            dst        -= n->idlen;
            ::memcpy(dst, n->id, n->idlen);
            *(--dst)   = cSeparator;
        }

        return dst;
    }

    KVTStorage::kvt_node_t *KVTStorage::create_node(kvt_node_t *base, const char *name, size_t len)
    {
        kvt_node_t *node;
        ssize_t first = 0, last = base->nchildren-1;

        // Seek for existing node
        while (first <= last)
        {
            ssize_t middle      = (first + last) >> 1;
            node                = base->children[middle];

            // Compare strings
            ssize_t cmp         = len - node->idlen;
            if (cmp == 0)
                cmp                 = ::memcmp(name, node->id, len);

            // Check result
            if (cmp < 0)
                last    = middle - 1;
            else if (cmp > 0)
                first   = middle + 1;
            else // Node does exist?
                return node;
        }

        // Create new node and add to the tree
        node        = allocate_node(name, len);
        if (node == NULL)
            return NULL;

        // Add to list of children
        if (base->nchildren >= base->capacity)
        {
            size_t ncap         = base->capacity + (base->capacity >> 1);
            if (ncap <= 0)
                ncap                = 0x10;
            kvt_node_t **rmem   = reinterpret_cast<kvt_node_t **>(::realloc(base->children, ncap * sizeof(kvt_node_t *)));
            if (rmem == NULL)
                return NULL;

            base->children      = rmem;
            base->capacity      = ncap;
        }

        // Link node to parent
        ::memmove(&base->children[first + 1], &base->children[first], sizeof(kvt_node_t *) * (base->nchildren - first));
        base->children[first]   = node;
        node->parent            = base;
        ++base->nchildren;
        reference_up(base);

        // Return node
        return node;
    }

    KVTStorage::kvt_node_t *KVTStorage::get_node(kvt_node_t *base, const char *name, size_t len)
    {
        kvt_node_t *node        = NULL;
        ssize_t first = 0, last = base->nchildren-1;

        // Seek for existing node
        while (first <= last)
        {
            ssize_t middle      = (first + last) >> 1;
            node                = base->children[middle];

            // Compare strings
            ssize_t cmp         = len - node->idlen;
            if (cmp == 0)
                cmp                 = ::memcmp(name, node->id, len);

            // Check result
            if (cmp < 0)
                last    = middle - 1;
            else if (cmp > 0)
                first   = middle + 1;
            else
                return node;
        }

        return NULL;
    }

    KVTStorage::kvt_gcparam_t *KVTStorage::copy_parameter(const kvt_param_t *src, size_t flags)
    {
        kvt_gcparam_t *gcp  = reinterpret_cast<kvt_gcparam_t *>(::malloc(sizeof(kvt_gcparam_t)));
        gcp->next           = NULL;

        kvt_param_t *dst    = gcp;
        *dst = *src;

        if (flags & KVT_DELEGATE)
            return gcp;

        // Need to override pointers
        if (src->type == KVT_STRING)
        {
            if (src->str != NULL)
            {
                if (!(dst->str = ::strdup(src->str)))
                {
                    ::free(gcp);
                    return NULL;
                }
            }
        }
        else if (src->type == KVT_BLOB)
        {
            if (src->blob.ctype != NULL)
            {
                if (!(dst->blob.ctype = ::strdup(src->blob.ctype)))
                {
                    ::free(gcp);
                    return NULL;
                }
            }
            if (src->blob.data != NULL)
            {
                if (!(dst->blob.data = ::malloc(src->blob.size)))
                {
                    if (dst->blob.ctype != NULL)
                        ::free(const_cast<char *>(dst->blob.ctype));
                    ::free(gcp);
                    return NULL;
                }
                ::memcpy(const_cast<void *>(dst->blob.data), src->blob.data, src->blob.size);
            }
        }

        return gcp;
    }

    status_t KVTStorage::commit_parameter(const char *name, kvt_node_t *node, const kvt_param_t *value, size_t flags)
    {
        kvt_gcparam_t *copy, *curr = node->param;

        // There is no current parameter?
        if (curr == NULL)
        {
            // Copy parameter
            if (!(copy = copy_parameter(value, flags)))
                return STATUS_NO_MEM;

            if (flags & KVT_COMMIT)
            {
                mark_clean(node);
                notify_commit(name, copy);
            }
            else
            {
                mark_dirty(node);
                notify_created(name, copy);
            }

            // Store pointer to the copy
            reference_up(node);
            node->param     = copy;
            ++nValues;

            return STATUS_OK;
        }

        // The node already contains parameter, need to do some stuff for replacing it
        // Do we need to keep old value?
        if (flags & KVT_KEEP)
        {
            notify_rejected(name, value, curr);
            return STATUS_ALREADY_EXISTS;
        }

        // Copy parameter
        if (!(copy = copy_parameter(value, flags)))
            return STATUS_NO_MEM;

        // Add previous value to trash, replace with new parameter
        curr->next          = pTrash;
        pTrash              = curr;
        node->param         = copy;

        if (flags & KVT_COMMIT)
        {
            mark_clean(node);
            notify_commit(name, copy);
        }
        else
        {
            mark_dirty(node);
            notify_changed(name, curr, copy);
        }

        return STATUS_OK;
    }

    status_t KVTStorage::put(const char *name, const kvt_param_t *value, size_t flags)
    {
        if ((name == NULL) || (value == NULL))
            return STATUS_BAD_ARGUMENTS;

        const char *path    = name;
        if ((value->type == KVT_ANY) || (*(path++) != cSeparator))
            return STATUS_INVALID_VALUE;

        kvt_node_t *curr = &sRoot;

        while (true)
        {
            const char *item = ::strchr(path, cSeparator);
            if (item != NULL) // It is a branch
            {
                // Estimate the length of the name
                size_t len  = item - path;
                if (!len) // Do not allow empty names
                    return STATUS_INVALID_VALUE;
                if (!(curr = create_node(curr, path, len)))
                    return STATUS_NO_MEM;
            }
            else // It is a leaf
            {
                size_t len  = ::strlen(path);
                if (!len) // Do not allow empty names
                    return STATUS_INVALID_VALUE;
                if (!(curr = create_node(curr, path, len)))
                    return STATUS_NO_MEM;

                // Commit the parameter
                return commit_parameter(name, curr, value, flags);
            }

            // Point to next after separator character
            path    = item + 1;
        }
    }

    status_t KVTStorage::walk_node(kvt_node_t **out, const char *name)
    {
        const char *path    = name;
        if (*(path++) != cSeparator)
            return STATUS_INVALID_VALUE;

        kvt_node_t *curr    = &sRoot;
        if (*path == '\0')
        {
            *out    = curr;
            return STATUS_OK;
        }

        while (true)
        {
            const char *item = ::strchr(path, cSeparator);
            if (item != NULL) // It is a branch
            {
                // Estimate the length of the name
                size_t len  = item - path;
                if (!len) // Do not allow empty names
                    return STATUS_INVALID_VALUE;

                curr = get_node(curr, path, len);
                if ((curr == NULL) || (curr->refs <= 0))
                    return STATUS_NOT_FOUND;
            }
            else // It is a leaf
            {
                size_t len  = ::strlen(path);
                if (!len) // Do not allow empty names
                    return STATUS_INVALID_VALUE;

                curr = get_node(curr, path, len);
                if ((curr == NULL) || (curr->refs <= 0))
                    return STATUS_NOT_FOUND;

                *out    = curr;
                return STATUS_OK;
            }

            // Point to next after separator character
            path    = item + 1;
        }
    }

    status_t KVTStorage::get(const char *name, const kvt_param_t **value, kvt_param_type_t type)
    {
        if (name == NULL)
            return STATUS_BAD_ARGUMENTS;

        // Find the leaf node
        kvt_node_t *node = NULL;
        status_t res = walk_node(&node, name);
        if (res != STATUS_OK)
        {
            if (res != STATUS_NOT_FOUND)
                return res;

            notify_missed(name);
            return res;
        }
        else if (node == &sRoot)
            return STATUS_INVALID_VALUE;

        kvt_gcparam_t *param = node->param;
        if (param == NULL)
        {
            // Notify listeners
            notify_missed(name);
            return STATUS_NOT_FOUND;
        }

        if ((type != KVT_ANY) && (type != param->type))
            return STATUS_BAD_TYPE;

        // All seems to be OK
        if (value != NULL)
        {
            *value  = param;
            notify_access(name, param);
        }
        return STATUS_OK;
    }

    bool KVTStorage::exists(const char *name, kvt_param_type_t type)
    {
        if (name == NULL)
            return false;

        // Find the leaf node
        kvt_node_t *node = NULL;
        status_t res = walk_node(&node, name);
        if (res != STATUS_OK)
        {
            if (res == STATUS_NOT_FOUND)
                notify_missed(name);
            return false;
        }
        if (node == &sRoot)
            return false;

        kvt_gcparam_t *param = node->param;
        if (param == NULL)
        {
            notify_missed(name);
            return false;
        }

        return ((type == KVT_ANY) || (type == param->type));
    }

    status_t KVTStorage::do_remove_node(const char *name, kvt_node_t *node, const kvt_param_t **value, kvt_param_type_t type)
    {
        kvt_gcparam_t *param = node->param;
        if (param == NULL)
        {
            notify_missed(name);
            return STATUS_NOT_FOUND;
        }

        if ((type != KVT_ANY) && (type != param->type))
            return STATUS_BAD_TYPE;

        // Add parameter to trash
        mark_clean(node);
        reference_down(node);
        param->next         = pTrash;
        pTrash              = param;
        node->param         = NULL;
        --nValues;

        notify_removed(name, param);

        // All seems to be OK
        if (value != NULL)
            *value  = param;
        return STATUS_OK;
    }

    status_t KVTStorage::remove(const char *name, const kvt_param_t **value, kvt_param_type_t type)
    {
        if (name == NULL)
            return STATUS_BAD_ARGUMENTS;

        // Find the leaf node
        kvt_node_t *node = NULL;
        status_t res = walk_node(&node, name);
        if (res != STATUS_OK)
        {
            if (res == STATUS_NOT_FOUND)
                notify_missed(name);
            return res;
        }
        else if (node == &sRoot)
            return STATUS_INVALID_VALUE;

        return do_remove_node(name, node, value, type);
    }

    status_t KVTStorage::do_touch(const char *name, kvt_node_t *node, bool modified)
    {
        // Parameter does exist?
        kvt_gcparam_t *param = node->param;
        if (param == NULL)
        {
            notify_missed(name);
            return STATUS_NOT_FOUND;
        }

        // Add parameter to trash
        if (modified)
        {
            mark_dirty(node);
            notify_changed(name, param, param);
        }
        else
        {
            mark_clean(node);
            notify_commit(name, param);
        }

        return STATUS_OK;
    }

    status_t KVTStorage::touch(const char *name, bool modified)
    {
        if (name == NULL)
            return STATUS_BAD_ARGUMENTS;

        // Find the leaf node
        kvt_node_t *node = NULL;
        status_t res = walk_node(&node, name);
        if (res != STATUS_OK)
        {
            if (res == STATUS_NOT_FOUND)
                notify_missed(name);
            return res;
        }
        else if (node == &sRoot)
            return STATUS_INVALID_VALUE;

        return do_touch(name, node, modified);
    }

    status_t KVTStorage::commit(const char *name)
    {
        return touch(name, false);
    }

    status_t KVTStorage::do_remove_branch(const char *name, kvt_node_t *node)
    {
        kvt_node_t *child;

        cvector<kvt_node_t> tasks;
        if (!tasks.add(node))
            return STATUS_NO_MEM;

        // Generate list of nodes for removal
        char *str = NULL, *path;
        size_t capacity = 0;

        while (tasks.size() > 0)
        {
            // Get the next task
            if (!tasks.pop(&node))
            {
                if (str != NULL)
                    ::free(str);
                return STATUS_UNKNOWN_ERR;
            }

            // Does node have a parameter?
            kvt_gcparam_t *param = node->param;
            if (param != NULL)
            {
                // Add parameter to trash
                mark_clean(node);
                reference_down(node);
                param->next         = pTrash;
                pTrash              = param;
                node->param         = NULL;
                --nValues;

                // Build path to node
                path = build_path(&str, &capacity, node);
                if (path == NULL)
                {
                    if (str != NULL)
                        ::free(str);
                    return STATUS_NO_MEM;
                }

                // Notify listeners
                notify_removed(path, param);
            }

            // Generate tasks for recursive search
            for (size_t i=0; i<node->nchildren; ++i)
            {
                child = node->children[i];
                if (child->refs <= 0)
                    continue;

                // Add child to the analysis list
                if (!tasks.push(child))
                {
                    if (str != NULL)
                        ::free(str);
                    return STATUS_NO_MEM;
                }
            }
        }

        if (str != NULL)
            ::free(str);

        return STATUS_OK;
    }

    status_t KVTStorage::remove_branch(const char *name)
    {
        if (name == NULL)
            return STATUS_BAD_ARGUMENTS;

        // Find the branch node
        kvt_node_t *node = NULL;
        status_t res = walk_node(&node, name);
        if (res != STATUS_OK)
            return res;

        return do_remove_branch(name, node);
    }

    status_t KVTStorage::bind(KVTListener *listener)
    {
        if (vListeners.index_of(listener) >= 0)
            return STATUS_ALREADY_BOUND;
        if (!vListeners.add(listener))
            return STATUS_NO_MEM;

        listener->attached(this);

        return STATUS_OK;
    }

    status_t KVTStorage::unbind(KVTListener *listener)
    {
        if (!vListeners.remove(listener))
            return STATUS_NOT_BOUND;

        listener->detached(this);

        return STATUS_OK;
    }

    status_t KVTStorage::is_bound(KVTListener *listener)
    {
        return vListeners.index_of(listener) >= 0;
    }

    status_t KVTStorage::unbind_all()
    {
        cvector<KVTListener> old;
        vListeners.swap_data(&old);

        for (size_t i=0, n=old.size(); i<n; ++i)
        {
            KVTListener *listener = old.at(i);
            if (listener != NULL)
                listener->detached(this);
        }

        return STATUS_OK;
    }

    status_t KVTStorage::put(const char *name, uint32_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_UINT32;
        param.u32   = value;
        return put(name, &param, flags | KVT_DELEGATE);
    }

    status_t KVTStorage::put(const char *name, int32_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_INT32;
        param.i32   = value;
        return put(name, &param, flags | KVT_DELEGATE);
    }

    status_t KVTStorage::put(const char *name, uint64_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_UINT64;
        param.u64   = value;
        return put(name, &param, flags | KVT_DELEGATE);
    }

    status_t KVTStorage::put(const char *name, int64_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_INT64;
        param.i64   = value;
        return put(name, &param, flags | KVT_DELEGATE);
    }

    status_t KVTStorage::put(const char *name, float value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_FLOAT32;
        param.f32   = value;
        return put(name, &param, flags | KVT_DELEGATE);
    }

    status_t KVTStorage::put(const char *name, double value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_FLOAT64;
        param.f64   = value;
        return put(name, &param, flags | KVT_DELEGATE);
    }

    status_t KVTStorage::put(const char *name, const char *value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_STRING;
        param.str   = const_cast<char *>(value);
        return put(name, &param, flags);
    }

    status_t KVTStorage::put(const char *name, const kvt_blob_t *value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_BLOB;
        param.blob  = *value;
        return put(name, &param, flags);
    }

    status_t KVTStorage::put(const char *name, size_t size, const char *type, const void *value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_BLOB;
        param.blob.size     = size;
        param.blob.ctype    = type;
        param.blob.data     = value;
        return put(name, &param, flags);
    }

    status_t KVTStorage::get(const char *name, uint32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_UINT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->u32;
        return res;
    }

    status_t KVTStorage::get(const char *name, int32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_INT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->i32;
        return res;
    }

    status_t KVTStorage::get(const char *name, uint64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_UINT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->u64;
        return res;
    }

    status_t KVTStorage::get(const char *name, int64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_INT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->i64;
        return res;
    }

    status_t KVTStorage::get(const char *name, float *value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_FLOAT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->f32;
        return res;
    }

    status_t KVTStorage::get(const char *name, double *value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_FLOAT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->f64;
        return res;
    }

    status_t KVTStorage::get(const char *name, const char **value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_STRING);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->str;
        return res;
    }

    status_t KVTStorage::get(const char *name, const kvt_blob_t **value)
    {
        const kvt_param_t *param;
        status_t res        = get(name, &param, KVT_BLOB);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = &param->blob;
        return res;
    }

    status_t KVTStorage::remove(const char *name, uint32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_UINT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->u32;
        return res;
    }

    status_t KVTStorage::remove(const char *name, int32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_INT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->i32;
        return res;
    }

    status_t KVTStorage::remove(const char *name, uint64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_UINT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->u64;
        return res;
    }

    status_t KVTStorage::remove(const char *name, int64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_INT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->i64;
        return res;
    }

    status_t KVTStorage::remove(const char *name, float *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_FLOAT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->f32;
        return res;
    }

    status_t KVTStorage::remove(const char *name, double *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_FLOAT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->f64;
        return res;
    }

    status_t KVTStorage::remove(const char *name, const char **value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_STRING);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->str;
        return res;
    }

    status_t KVTStorage::remove(const char *name, const kvt_blob_t **value)
    {
        const kvt_param_t *param;
        status_t res        = remove(name, &param, KVT_BLOB);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = &param->blob;
        return res;
    }

    void KVTStorage::destroy_node(kvt_node_t *node)
    {
        node->id        = NULL;
        node->idlen     = 0;
        node->parent    = NULL;

        if (node->param != NULL)
        {
            destroy_parameter(node->param);
            node->param     = NULL;
        }
        node->modified  = false;

        if (node->children != NULL)
        {
            ::free(node->children);
            node->children = NULL;
        }
        node->nchildren = 0;
        node->capacity  = 0;

        ::free(node);
    }

    status_t KVTStorage::gc()
    {
        // Part 0: Destroy all iterators
        while (pIterators != NULL)
        {
            KVTIterator *next   = pIterators->pGcNext;
            delete pIterators;
            pIterators          = next;
        }

        // Part 1: Collect all garbage parameters
        while (pTrash != NULL)
        {
            kvt_gcparam_t *next = pTrash->next;
            destroy_parameter(pTrash);
            pTrash      = next;
        }

        // Part 2: Unlink all garbage nodes from valid parents
        for (kvt_link_t *lnk = sGarbage.next; lnk != NULL; lnk = lnk->next)
        {
            kvt_node_t *node = lnk->node;
            kvt_node_t *parent = node->parent;
            if (parent->refs <= 0)
                continue;

            for (size_t i=0; i<parent->nchildren; )
            {
                if (parent->children[i]->refs <= 0)
                {
                    --parent->nchildren;
                    ::memmove(&parent->children[i],
                            &parent->children[i+1],
                            (parent->nchildren - i) * sizeof(kvt_node_t *));
                }
                else
                    ++i;
            }
        }

        // Part 3: Collect garbage nodes
        while (sGarbage.next != NULL)
        {
            kvt_node_t *node = sGarbage.next->node;
            unlink_list(&node->mod);
            unlink_list(&node->gc);

            destroy_node(node);
        }

        return STATUS_OK;
    }

    KVTIterator *KVTStorage::enum_modified()
    {
        kvt_link_t *lnk = sDirty.next;
        return new KVTIterator(this, (lnk != NULL) ? lnk->node : NULL, IT_MODIFIED);
    }

    KVTIterator *KVTStorage::enum_branch(const char *name, bool recursive)
    {
        // Find the leaf node
        kvt_node_t *node = NULL;
        status_t res = walk_node(&node, name);
        if (res != STATUS_OK)
        {
            if (res == STATUS_NOT_FOUND)
                notify_missed(name);
        }

        return new KVTIterator(this, node, (recursive) ? IT_RECURSIVE : IT_BRANCH);
    }


    KVTIterator::KVTIterator(KVTStorage *storage, KVTStorage::kvt_node_t *node, KVTStorage::iterator_mode_t mode)
    {
        sFake.id        = NULL;
        sFake.idlen     = 0;
        sFake.parent    = node;
        sFake.refs      = 0;
        sFake.children  = NULL;
        sFake.param     = NULL;
        sFake.modified  = false;
        sFake.gc.next   = NULL;
        sFake.gc.prev   = NULL;
        sFake.mod.next  = (node != NULL) ? &node->mod : NULL;
        sFake.mod.prev  = NULL;
        sFake.children  = NULL;
        sFake.nchildren = 0;
        sFake.capacity  = 0;

        enMode          = mode;
        pCurr           = &sFake;
        pNext           = node;
        nIndex          = ~size_t(0);
        pPath           = NULL;
        pData           = NULL;
        nDataCap        = 0;
        pStorage        = storage;

        pGcNext         = storage->pIterators;
        storage->pIterators = this; // Link to the garbage
    }

    KVTIterator::~KVTIterator()
    {
        pCurr           = NULL;
        nIndex          = 0;
        vPath.flush();
        enMode          = KVTStorage::IT_INVALID;
        pPath           = NULL;

        if (pData != NULL)
        {
            ::free(pData);
            pData           = NULL;
        }

        nDataCap        = 0;
        pGcNext         = NULL;
        pStorage        = NULL;
    }

    bool KVTIterator::valid() const
    {
        return (pCurr != &sFake) && (pCurr != NULL) && (pCurr->refs > 0);
    }

    bool KVTIterator::modified() const
    {
        return (valid()) && (pCurr->modified);
    }

    const char *KVTIterator::id() const
    {
        return (valid()) ? pCurr->id : NULL;
    }

    const char *KVTIterator::name() const
    {
        if (!valid())
            return NULL;

        if (pPath == NULL)
            pPath = pStorage->build_path(&pData, &nDataCap, pCurr);

        return pPath;
    }

    status_t KVTIterator::next()
    {
        // Invalidate current path
        pPath       = NULL;

        switch (enMode)
        {
            case KVTStorage::IT_MODIFIED:
            {
                KVTStorage::kvt_link_t *lnk;

                pCurr       = pNext;
                if ((pCurr == NULL) || (!pCurr->modified))
                {
                    enMode      = KVTStorage::IT_EOF;
                    return STATUS_NOT_FOUND;
                }

                lnk         = (pCurr != NULL) ? pCurr->mod.next : NULL;
                pNext       = (lnk != NULL) ? lnk->node : NULL;
                break;
            }

            case KVTStorage::IT_BRANCH:
            {
                do
                {
                    if ((++nIndex) >= pCurr->parent->nchildren)
                    {
                        enMode      = KVTStorage::IT_EOF;
                        return STATUS_NOT_FOUND;
                    }
                    pCurr       = pCurr->parent->children[nIndex];
                } while (pCurr->refs <= 0);
                break;
            }

            case KVTStorage::IT_RECURSIVE:
            {
                do
                {
                    if (pCurr->nchildren > 0)
                    {
                        kvt_path_t *path = vPath.push();
                        if (path == NULL)
                            return STATUS_NO_MEM;
                        path->index = nIndex + 1;
                        path->node  = pCurr;
                        pCurr       = pCurr->children[0];
                        nIndex      = 0;
                    }
                    else
                    {
                        if ((++nIndex) >= pCurr->parent->nchildren)
                        {
                            do {
                                kvt_path_t path;
                                if (!vPath.pop(&path))
                                {
                                    enMode      = KVTStorage::IT_EOF;
                                    return STATUS_NOT_FOUND;
                                }
                                nIndex      = path.index;
                                pCurr       = pCurr->parent;
                            } while (nIndex >= pCurr->parent->nchildren);
                        }
                        pCurr       = pCurr->parent->children[nIndex];
                    }
                } while (pCurr->refs <= 0);

                break;
            }

            case KVTStorage::IT_EOF:
                return STATUS_NOT_FOUND;

            default:
                return STATUS_BAD_STATE;
        }

        return STATUS_OK;
    }

    bool KVTIterator::exists(kvt_param_type_t type) const
    {
        if (!valid())
            return false;

        KVTStorage::kvt_gcparam_t *param = pCurr->param;
        if (param == NULL)
        {
            // Get parameter name
            const char *id = name();
            if (id == NULL)
                return false;

            pStorage->notify_missed(id);
            return false;
        }

        return ((type == KVT_ANY) || (type == param->type));
    }

    status_t KVTIterator::get(const kvt_param_t **value, kvt_param_type_t type)
    {
        if (!valid())
            return STATUS_BAD_STATE;

        const char *id = name();
        if (id == NULL)
            return STATUS_NO_MEM;

        KVTStorage::kvt_gcparam_t *param = pCurr->param;
        if (param == NULL)
        {
            pStorage->notify_missed(id);
            return STATUS_NOT_FOUND;
        }

        if ((type != KVT_ANY) && (type != param->type))
            return STATUS_BAD_TYPE;

        // All seems to be OK
        if (value != NULL)
        {
            *value  = param;
            pStorage->notify_access(id, param);
        }
        return STATUS_OK;
    }

    status_t KVTIterator::get(uint32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_UINT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->u32;
        return res;
    }

    status_t KVTIterator::get(int32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_INT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->i32;
        return res;
    }

    status_t KVTIterator::get(uint64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_UINT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->u64;
        return res;
    }

    status_t KVTIterator::get(int64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_INT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->i64;
        return res;
    }

    status_t KVTIterator::get(float *value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_FLOAT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->f32;
        return res;
    }

    status_t KVTIterator::get(double *value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_FLOAT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->f64;
        return res;
    }

    status_t KVTIterator::get(const char **value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_STRING);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = param->str;
        return res;
    }

    status_t KVTIterator::get(const kvt_blob_t **value)
    {
        const kvt_param_t *param;
        status_t res        = get(&param, KVT_BLOB);
        if ((res == STATUS_OK) && (value != NULL))
            *value      = &param->blob;
        return res;
    }

    status_t KVTIterator::put(const kvt_param_t *value, size_t flags)
    {
        if (!valid())
            return false;

        const char *id = name();
        if (id == NULL)
            return STATUS_NO_MEM;

        return pStorage->commit_parameter(id, pCurr, value, flags);
    }

    status_t KVTIterator::put(uint32_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_UINT32;
        param.u32   = value;
        return put(&param, flags | KVT_DELEGATE);
    }

    status_t KVTIterator::put(int32_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_INT32;
        param.i32   = value;
        return put(&param, flags | KVT_DELEGATE);
    }

    status_t KVTIterator::put(uint64_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_UINT64;
        param.u64   = value;
        return put(&param, flags | KVT_DELEGATE);
    }

    status_t KVTIterator::put(int64_t value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_INT64;
        param.i64   = value;
        return put(&param, flags | KVT_DELEGATE);
    }

    status_t KVTIterator::put(float value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_FLOAT32;
        param.f32   = value;
        return put(&param, flags | KVT_DELEGATE);
    }

    status_t KVTIterator::put(double value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_FLOAT64;
        param.f64   = value;
        return put(&param, flags | KVT_DELEGATE);
    }

    status_t KVTIterator::put(const char *value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_STRING;
        param.str   = const_cast<char *>(value);
        return put(&param, flags);
    }

    status_t KVTIterator::put(const kvt_blob_t *value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_BLOB;
        param.blob  = *value;
        return put(&param, flags);
    }

    status_t KVTIterator::put(size_t size, const char *type, const void *value, size_t flags)
    {
        kvt_param_t param;
        param.type  = KVT_BLOB;
        param.blob.size     = size;
        param.blob.ctype    = type;
        param.blob.data     = value;
        return put(&param, flags);
    }

    status_t KVTIterator::remove(const kvt_param_t **value, kvt_param_type_t type)
    {
        if (!valid())
            return STATUS_BAD_STATE;

        const char *id = name();
        if (id == NULL)
            return STATUS_NO_MEM;

        return pStorage->do_remove_node(id, pCurr, value, type);
    }

    status_t KVTIterator::remove(uint32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_UINT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->u32;
        return res;
    }

    status_t KVTIterator::remove(int32_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_INT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->i32;
        return res;
    }

    status_t KVTIterator::remove(uint64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_UINT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->u64;
        return res;
    }

    status_t KVTIterator::remove(int64_t *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_INT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->i64;
        return res;
    }

    status_t KVTIterator::remove(float *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_FLOAT32);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->f32;
        return res;
    }

    status_t KVTIterator::remove(double *value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_FLOAT64);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->f64;
        return res;
    }

    status_t KVTIterator::remove(const char **value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_STRING);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = param->str;
        return res;
    }

    status_t KVTIterator::remove(const kvt_blob_t **value)
    {
        const kvt_param_t *param;
        status_t res        = remove(&param, KVT_BLOB);
        if ((res == STATUS_OK) && (value != NULL))
            *value              = &param->blob;
        return res;
    }

    status_t KVTIterator::touch(bool modified)
    {
        if (!valid())
            return STATUS_BAD_STATE;

        const char *id = name();
        if (id == NULL)
            return STATUS_NO_MEM;

        return pStorage->do_touch(id, pCurr, false);
    }

    status_t KVTIterator::commit()
    {
        if (!valid())
            return STATUS_BAD_STATE;

        const char *id = name();
        if (id == NULL)
            return STATUS_NO_MEM;

        return pStorage->do_touch(id, pCurr, false);
    }

    status_t KVTIterator::remove_branch()
    {
        if (!valid())
            return STATUS_BAD_STATE;

        const char *id = name();
        if (id == NULL)
            return STATUS_NO_MEM;

        return pStorage->do_remove_branch(id, pCurr);
    }

} /* namespace lsp */
