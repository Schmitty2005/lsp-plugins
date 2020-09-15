/*
 * Copyright (C) 2020 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2020 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 29 апр. 2018 г.
 *
 * lsp-plugins is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef UI_CTL_CTLCOMBOGROUP_H_
#define UI_CTL_CTLCOMBOGROUP_H_

namespace lsp
{
    namespace ctl
    {
        
        class CtlComboGroup: public CtlWidget
        {
            public:
                static const ctl_class_t metadata;

            protected:
                CtlPort        *pPort;
                float           fMin;
                float           fMax;
                float           fStep;
                char           *pText;

                ui_handler_id_t idChange;

                CtlColor        sColor;
                CtlColor        sTextColor;
                CtlExpression   sEmbed;

            protected:
                static status_t slot_change(LSPWidget *sender, void *ptr, void *data);

                void submit_value();
                void do_destroy();

            public:
                explicit CtlComboGroup(CtlRegistry *src, LSPComboGroup *widget);
                virtual ~CtlComboGroup();

            public:
                virtual void init();

                virtual void destroy();

                virtual void set(widget_attribute_t att, const char *value);

                virtual status_t add(CtlWidget *child);

                virtual void notify(CtlPort *port);

                virtual void end();
        };
    
    } /* namespace ctl */
} /* namespace lsp */

#endif /* UI_CTL_COMBOGROUP_H_ */
