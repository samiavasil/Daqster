TEMPLATE = subdirs
#BUILD_TEST_PLUGINS = TESTS

SUBDIRS += QtCoinTrader
SUBDIRS += node_editor
CONFIG+= c++11

if( defined( BUILD_TEST_PLUGINS,var ) ){
 SUBDIRS +=  tests/template_plugin_daqster\
             tests/plugin_fancy_test \
             tests/plugin_main_test \
             tests/plugin_uggly_test \

}
