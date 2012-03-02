# Generated by GOLD Parser Builder using MyBuild program template.

# Rule productions for 'ConfigFile' grammar.

#
# As for symbols each rule can have a constructor that is used to produce an
# application-specific representation of the rule data.
# Production functions are named '$(gold_grammar)_produce-<ID>' and have the
# following signature:
#
# Params:
#   1..N: Each argument contains a value of the corresponding symbol in the RHS
#         of the rule production.
#
# Return:
#   Converted value that is passed to a symbol handler corresponding to
#   the rule's LHS (if any has been defined).
#
# If production function is not defined then the rule is produced by
# concatenating the RHS through spaces. To reuse this default value one can
# call 'gold_default_produce' function.
#

# Rule: <ConfigFile> ::= <Package> <Imports> <Configurations>
# Args: 1..3 - Symbols in the RHS.
define $(gold_grammar)_produce-ConfigFile
	$(for fileContent <- $(new CfgFileContent),
		$(set fileContent->name,$1)
		$(set fileContent->imports,$2)
		$(set fileContent->configurations,$3)
		$(fileContent)
	)
endef

# Rule: <Package> ::= package <QualifiedName>
# Args: 1..2 - Symbols in the RHS.
$(gold_grammar)_produce-Package_package  = $2

# Rule: <Package> ::=
define $(gold_grammar)_produce-Package
	$(call gold_report_warning,
			Using default package)
endef

# Rule: <Import> ::= import <ImportFeature> <QualifiedNameWithWildcard>
$(gold_grammar)_produce-Import_import = $3

# Rule: <Configuration> ::= configuration Identifier '{' <ConfigurationMembers> '}'
# Args: 1..5 - Symbols in the RHS.
define $(gold_grammar)_produce-Configuration_configuration_Identifier_LBrace_RBrace
	$(for cfg <- $(new CfgConfiguration),
		$(set cfg->name,$2)
		$(set cfg->origin,$(call gold_location_of,2))

		$(silent-foreach member,
				includes,
				$(set cfg->$(member),\
					$(filter-patsubst $(member)/%,%,$4)))

		$(cfg)
	)
endef

# Rule: <Include> ::= include <ModuleRefList>
define $(gold_grammar)_produce-Include_include
	$(for link <- $2,
		include <- $(new CfgInclude),

		$(set include->module_link,$(link))

		$1s/$(include)
	)
endef

# <ModuleRef> ::= <QualifiedName>
$(gold_grammar)_produce-ModuleRef    = $(new ELink,$1,$(gold_location))

# <ModuleRefList> ::= <ModuleRef> ',' <ModuleRefList>
$(gold_grammar)_produce-ModuleRefList_Comma        = $1 $3

# <QualifiedName> ::= Identifier '.' <QualifiedName>
$(gold_grammar)_produce-QualifiedName_Identifier_Dot         = $1.$3
# <QualifiedNameWithWildcard> ::= <QualifiedName> '.*'
$(gold_grammar)_produce-QualifiedNameWithWildcard_DotTimes   = $1.*


$(def_all)

