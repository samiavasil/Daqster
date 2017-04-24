TEMPLATE = subdirs

BUILD_TEST_PLUGINS = TESTS
SUBDIRS += template_plugin_daqster

if( defined( BUILD_TEST_PLUGINS,var ) ){
 SUBDIRS +=  tests/plugin_fancy_test \
             tests/plugin_main_test \
             tests/plugin_uggly_test
}
