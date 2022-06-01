<?php
/*
Plugin Name: PrimusMoney POCs
Plugin URI: https://www.primusmoney.com/
Description: Plug-in to demonstrate primus money payment widget via POCs.
Version: 0.10.01
Author: PrimusMoney
Author URI: https://www.primusmoney.com
License:
Text Domain: PrimusMoneyWordpressPOCs
*/
defined( 'ABSPATH' ) or die( 'Nope, not accessing this' );


// Make sure PrimusMoneyPayments is active
$need = false;
         
if ( ! function_exists( 'is_plugin_active_for_network' ) ) {
  require_once( ABSPATH . '/wp-admin/includes/plugin.php' );
}
     
// handling multisite mode
if ( is_multisite() ) {
  // this plugin is network activated - PrimusMoneyPayments must be network activated 
  if ( is_plugin_active_for_network( plugin_basename(__FILE__) ) ) {
    $need = is_plugin_active_for_network('primusmoney-payments/primusmoney-payments.php') ? false : true; 
  // this plugin is locally activated - PrimusMoneyPayments can be network or locally activated 
  } else {
    $need = is_plugin_active( 'primusmoney-payments/primusmoney-payments.php')  ? false : true;   
  }
// this plugin runs on a single site    
} else {
  $need =  is_plugin_active( 'primusmoney-payments/primusmoney-payments.php') ? false : true;     
}
     
if ($need === true) {
  add_action( 'admin_notices', 'need_primusmoney_payments' );
  return; 
}

function need_primusmoney_payments() {
	$error = sprintf( __( 'The plugin PrimusMoney POCs requires PrimusMoney Payments. Please install and activate this plugin. ' , 'primusmoney-wordpress-pocs' ), '<a href="http://wordpress.org/extend/plugins/woocommerce/">', '</a>' );
	$message = '<div class="error"><p>' . $error . '</p></div>';
	echo $message;
  }

// log function
if ( ! function_exists('primus_debug_log')) {
	function primus_debug_log ( $log )  {
		if (!defined('WP_DEBUG') || (WP_DEBUG !== true))
		return;

		if ( is_array( $log ) || is_object( $log ) ) {
			error_log( print_r( $log, true ) );
		} else {
			error_log( $log );
		}
	}
}

class PrimusMoneyWordpressPOCs {
	private static $current_version = '0.10.01.2022.05.29';
	private static $exec_env = 'dev';
	
	private static $instance;

	public static function current_version() {
		return self::$current_version;
	}

	public static function execution_environment() {
		return self::$exec_env;
	}



	public static function get_instance() {
		if ( self::$instance === null ) {
			self::$instance = new self();
		}
		return self::$instance;
	}

	private $poc_name;

	private function __construct() {

		$this->rest_namespace = 'primusmoney/wppocs/v1';

		// initialization of plugin
		add_action( 'init', array( $this, 'init' ));

		// load of plugin
		add_action( 'plugins_loaded', array( $this, 'plugins_loaded' ));

		// callback and REST
		add_action( 'rest_api_init', array( $this, 'rest_api_register_routes' ), 11 );

		
	}

	public function init() {
		primus_debug_log('PrimusMoneyWordpressPOCs::init called');

		$current_url = home_url( $_SERVER['REQUEST_URI'] );
		$url_arr = parse_url($current_url);
		$url_path = $url_arr['path'];

		if (strpos($url_path, 'invoice') !== false)
			$this->poc_name = 'invoice';
		else
			$this->poc_name = 'default-wp-pocs';

		primus_debug_log('poc is: '.(isset($this->poc_name) ? $this->poc_name : '' ));

		switch($this->poc_name) {

			case 'invoice':
				require_once(dirname(__FILE__) .'/invoice/invoice.inc');
				break;

			default:
				break;
		}

		// add common actions and filters
		add_filter( 'primusmoney_get_json_config', array( $this, 'get_json_config' ), 10, 2  );


	}

	public function plugins_loaded() {
		primus_debug_log('PrimusMoneyWordpressPOCs::plugins_loaded called');
	}

	private function _getPocJsonConfig($configname) {
		if (isset($this->poc_name)) {
			$_name = $this->poc_name.'/'.$configname;
			$config = primus_get_json_file($_name);
	
			return $config;
		}		
	}

	public function get_json_config($config, $configname ) {
		primus_debug_log('PrimusMoneyWordpressPOCs::get_json_config called for '.$configname);

		// look if file exists in sub-folder
		$_new_config = $this->_getPocJsonConfig($configname);
	
		if (!empty($_new_config)) {
			$config = $_new_config;
		}

		return $config;
	}

	// REST api
	public function _api_get_version($request) {
		primus_debug_log('PrimusMoneyWordpressPOCs::_api_get_version called' );
		$plugin = $this;
		$current_version = $plugin->current_version();

		$answer = array(
			'version' => $current_version
		);

		return $answer;
	}

	// invoice POC rest api
	public function _api_read_invoice($request) {
		primus_debug_log('PrimusMoneyWordpressPOCs::_api_read_invoice called' );
		
		$invoice_inst = InvoicePOC::get_instance();

		return $invoice_inst->_api_read_invoice($request);
	}


	// rest declarations
	public function rest_api_url() {
		return get_rest_url(null, $this->rest_namespace);
	}

	public function rest_api_register_routes() {
		primus_debug_log('PrimusMoneyWordpressPOCs::rest_api_register_routes called' );

		$namespace = $this->rest_namespace;

		register_rest_route(
			$namespace,
			'/version',
			array(
				array(
					'methods'             => 'GET',
					'callback'            => array( $this, '_api_get_version' ),
					'permission_callback' => '__return_true',
				),
			)
		);

		register_rest_route(
			$namespace,
			'/invoice/read',
			array(
				array(
					'methods'             => 'POST',
					'callback'            => array( $this, '_api_read_invoice' ),
					'permission_callback' => '__return_true',
				),
			)
		);


	}


	// utils

	public function plugin_root_dir() {
		return plugin_dir_url( __FILE__ );
	}

	public function plugin_basename() {
		return plugin_basename(__FILE__);
	}
		
	
}


// instantiation
$plugin = PrimusMoneyWordpressPOCs::get_instance();

