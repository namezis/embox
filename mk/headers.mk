
include mk/mybuild/resource.mk
include core/define.mk
HEADERS_BUILD := \
  $(patsubst %,$(OBJ_DIR)/mods/%.h,$(subst .,/,$(basename $(APIS_BUILD))))

$(warning HEADERS_BUILD is $(HEADERS_BUILD))
$(warning APIS is $(APIS_BUILD))

__header_mod = \
  $(subst /,.,$(patsubst $(abspath $(OBJ_DIR))/mods/%.h,%,$(abspath $@)))

__header_gen = \
  $(subst $(\n),\n,$(call __header_template,$(__header_mod),$(call find_mod,$(__header_mod))))

#param $1 is module obj
#param $2 is module list
#output is module descedant from list
define find_descedant
	$(info find_descedant $1)
	$(for o<-$1,
		m<-$2,
		$(with $m,
			$(for l<-$(get $1.super_module_ref),
				super<-$(invoke l->get_reference),
				$(if $(eq $(suffix $(super)),$(suffix $o)),
					$(info found $m)$m,
					$(call $0,$(super))))))
endef

$(call def,find_descedant)

# 1. Header module name
# 2. Header module obj
define __header_template
/* Autogenerated file. Do not edit. */

#ifndef __MOD_HEADER__$(subst .,__,$1)
#define __MOD_HEADER__$(subst .,__,$1)
$(foreach impl,$(call find_descedant,$2,$(MODS_ENABLE_OBJ)),$(\n)// impl: $(impl)$ \
  $(foreach header,$(strip $(patsubst $(abspath $(SRC_DIR))/%,%,
                 $(abspath $(call module_get_headers,$(impl))))),$ \
      $(\n)$(\h)include __impl_x($(header))))

#endif /* __MOD_HEADER__$(subst .,__,$1) */

endef

$(HEADERS_BUILD): $(EMBUILD_DUMP_PREREQUISITES) $(MK_DIR)/image.mk $(AUTOCONF_DIR)/mods.mk
	@$(MKDIR) $(@D) && printf "%b" '$(__header_gen)' > $@
