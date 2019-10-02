/*
 * LSPStyle.h
 *
 *  Created on: 1 окт. 2019 г.
 *      Author: sadko
 */

#ifndef UI_TK_SYS_LSPSTYLE_H_
#define UI_TK_SYS_LSPSTYLE_H_

namespace lsp
{
    namespace tk
    {
        class LSPColor;
        
        /**
         * Style Listener
         */
        class ILSPStyleListener
        {
            public:
                virtual ~ILSPStyleListener();

            public:
                virtual void notify(ui_atom_t property);
        };

        /**
         * Some widget style. Allows nesting
         */
        class LSPStyle
        {
            protected:
                typedef struct property_t
                {
                    ui_atom_t           id;         // Unique identifier of property
                    ssize_t             type;       // Type of property
                    size_t              refs;       // Number of references
                    bool                dfl;        // Default value
                    union
                    {
                        ssize_t     iValue;
                        float       fValue;
                        bool        bValue;
                        char       *sValue;
                    } v;
                } property_t;

                typedef struct listener_t
                {
                    property_t         *pProperty;
                    ILSPStyleListener  *pListener;
                } listener_t;

            private:
                LSPStyle               *pParent;
                cvector<LSPStyle>       vChildren;
                cstorage<property_t>    vProperties;
                cstorage<listener_t>    vListeners;

            public:
                explicit LSPStyle();
                virtual ~LSPStyle();

                status_t            init();
                void                destroy();

            protected:
                void                undef_property(property_t *property);
                void                do_destroy();
                property_t         *get_property_recursive(ui_atom_t id);
                property_t         *get_property(ui_atom_t id);
                status_t            set_property(ui_atom_t id, property_t *src);
                inline const property_t   *get_property_recursive(ui_atom_t id) const { return const_cast<LSPStyle *>(this)->get_property_recursive(id); };
                inline const property_t   *get_property(ui_atom_t id) const { return const_cast<LSPStyle *>(this)->get_property(id); };

            public:
                /**
                 * Add child style
                 * @param child child style
                 * @return status of operation
                 */
                status_t            add(LSPStyle *child);

                /** Remove child style
                 *
                 * @param child child style to remove
                 * @return status of operation
                 */
                status_t            remove(LSPStyle *child);

                /**
                 * Set parent style
                 * @param parent parent style
                 * @return statys of operation
                 */
                status_t            set_parent(LSPStyle *parent);

                /**
                 * Get parent style
                 * @return parent style
                 */
                inline LSPStyle    *parent()        { return pParent; };

                /**
                 * Get root style
                 * @return root style
                 */
                LSPStyle           *root();

            public:
                /**
                 * Bind listener to property
                 * @param id property identifier
                 * @return status of operation
                 */
                status_t            bind(ui_atom_t id, ui_property_type_t type, ILSPStyleListener *listener);

                /**
                 * Unbind listener from a property
                 * @param id property identifier
                 * @param listener property listener
                 * @return status of operation
                 */
                status_t            unbind(ui_atom_t id, ILSPStyleListener *listener);

            public:
                status_t            get_int(ui_atom_t id, ssize_t *dst) const;
                status_t            get_float(ui_atom_t id, float *dst) const;
                status_t            get_bool(ui_atom_t id, bool *dst) const;
                status_t            get_string(ui_atom_t id, LSPString *dst) const;
                bool                exists(ui_atom_t id) const;
                bool                is_default(ui_atom_t id) const;

                status_t            set_int(ui_atom_t id, ssize_t value);
                status_t            set_float(ui_atom_t id, float value);
                status_t            set_bool(ui_atom_t id, bool value);
                status_t            set_string(ui_atom_t id, const LSPString *value);
                status_t            set_default(ui_atom_t id);
        };
    
    } /* namespace tk */
} /* namespace lsp */

#endif /* UI_TK_SYS_LSPSTYLE_H_ */
