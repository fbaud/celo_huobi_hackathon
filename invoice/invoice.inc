<?php


class InvoicePOC {
	private static $instance;

	public static function get_instance() {
		if ( self::$instance === null ) {
			self::$instance = new self();
		}
		return self::$instance;
	}
	
	private function __construct() {
		primus_debug_log('InvoicePOC::__construct called');

		// callback and REST
		add_filter( 'primusmoney_payment_callback', array( $this, 'callback_handler' ), 10, 1 );


		// sliced_invoice
		add_action( 'sliced_invoice_after_totals_table', array( $this, 'after_totals_table' ));

		// short codes
		add_shortcode('primusmoney_pocs_invoice_list', array( $this, 'invoice_list'));

	}

	public function getJsDir() {
		$wp_script_url_dir = plugin_dir_url( __FILE__ );
		$wp_script_url_dir .= '../assets/js/invoice/';

		return $wp_script_url_dir;
	}

	public function getCssDir() {
		$wp_css_url_dir = plugin_dir_url( __FILE__ );
		$wp_css_url_dir .= '../assets/css/invoice/';

		return $wp_css_url_dir;
	}

	// callback
	public function callback_handler($answer) {
		primus_debug_log('InvoicePOC::callback_handler called');

		$actual_link = (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] === 'on' ? "https" : "http") . "://$_SERVER[HTTP_HOST]$_SERVER[REQUEST_URI]";
		primus_debug_log('callback url is: '.$actual_link);


		$bSuccess = TRUE;
		$data = array();
		$error = 'error occured';

		// what is the action to perform
		$action = (isset($_GET['action']) && !empty($_GET['action']) ? $_GET['action'] :'none');
		

		// default action is "payment_status" to register a transaction hash

		// invoice id
		$invoice_id = (isset($_GET['invoiceid']) && !empty($_GET['invoiceid']) ? $_GET['invoiceid'] : NULL);

		primus_debug_log('invoice id is: '.$invoice_id);

		// transaction hash
		$transaction_hash = (isset($_GET['tx']) && !empty( $_GET['tx']) ? $_GET['tx'] : NULL);

		primus_debug_log('transaction hash is: '.$transaction_hash);

		// check if it is a call to save a transaction hash
		if (isset($invoice_id) && isset($transaction_hash) && ($action == 'none')) {
			$action = 'payment_status';
		}
		
		
		switch($action) {
			case 'payment_status': {
				// save transaction hash
				$this->_saveTansactionHash($invoice_id, $transaction_hash);

				// update invoice status
				$this->_setInvoiceStatus($invoice_id,'paid');

				$data['invoiceid'] = $invoice_id;
				$data['tx'] = $transaction_hash;
			}
			break;

			default:
			break;
		}

		if ($bSuccess) {
			$answer['status'] = 1;
			$answer['data'] = $data;
		}
		else {
			$answer['status'] = 0;
			$answer['error'] = $error;
		}

		return $answer;
	}

	// POC rest api
	public function _api_read_invoice($request) {
		primus_debug_log('InvoicePOC::_api_read_invoice called' );
		
		$invoice_id = $request['invoice_id'];

		$invoice_info = $this->_getInvoiceInfo($invoice_id);

		$answer = (isset($invoice_info) ? $invoice_info : array());

		return $answer;
	}
	
	

	private function __find_sliced_post_id($invoice_id) {
		$args = array(
			'post_type'     =>  'sliced_invoice',
			'post_status'    => 'publish',
			'posts_per_page' => -1,
			'fields'     	 => 'ids',
			'orderby' => 'date',
			'order' => 'DESC',
		);

		$post_ids = get_posts($args);

		foreach($post_ids as $post_id) {
			$sliced_number = get_post_meta( $post_id, '_sliced_number');
			$inv_id = (isset($sliced_number) && is_array($sliced_number) && isset($sliced_number[0]) ? $sliced_number[0] : NULL);

			if ($inv_id == $invoice_id)
			return $post_id;
		}

		return -1;
	}

	private function _saveTansactionHash($invoice_id, $transaction_hash) {
		if (class_exists('Sliced_Shared')) {

			$post_id = $this->__find_sliced_post_id($invoice_id);

			if ($post_id == -1)
			return;

			$tx_meta = get_post_meta( $post_id, '_primusmoney_tx');

			if (!isset($tx_meta)) {
				add_post_meta( $post_id, '_primusmoney_tx', $transaction_hash);
			}
			else {
				update_post_meta( $post_id, '_primusmoney_tx', $transaction_hash);
			}
	
		}
	}

	private function _getInvoiceStatus($invoice_id) {

		if (class_exists('Sliced_Invoice')) {
			$post_id = $this->__find_sliced_post_id($invoice_id);

			if ($post_id == -1)
			return;

			$test_status = 'unpaid';

			if( has_term( 'paid', 'invoice_status', $post_id ) ) {
				$test_status = 'paid';
			}
			else if( has_term( 'cancelled', 'invoice_status', $post_id ) ) {
				$test_status = 'cancelled';
			}
			else if( has_term( 'overdue', 'invoice_status', $post_id ) ) {
				$test_status = 'overdue';
			}
			else if( has_term( 'draft', 'invoice_status', $post_id ) ) {
				$test_status = 'draft';
			}

			return $test_status;
/* 			$log_meta = get_post_meta( $post_id, '_sliced_log', true);

			$old_status = 'unpaid';
			$status_time = 0;

			// loop through each log entry to find status
			foreach ($log_meta as $time => $log) {
				// work out the type of log entry
				switch ($log['type']) {
					case 'status_update':
						if ($time > $status_time)
						$old_status = $log['to'];
						break;
					case 'marked_as_paid':
						if ($time > $status_time)
						$old_status = 'paid';
						break;

					default:
						break;
				}

			}

			return strtolower($old_status); */
		}
	
	}

	private function _setInvoiceStatus($invoice_id, $status) {
		$invoice_info = array();

		if (class_exists('Sliced_Invoice')) {
			$post_id = $this->__find_sliced_post_id($invoice_id);

			if ($post_id == -1)
			return;

			Sliced_Invoice::set_status( $status, $post_id );
			return TRUE;
		}
	}

	private function _isTransactionHashValid($tx_hash, $invoice_id) {
		if ($tx_hash && strlen($tx_hash)) {
			return true; // very simple validation for the moment
		}
		else
			return false;
	}

	private function _getInvoiceInfo($inv_id = NULL) {
		$invoice_info = array();

		if (class_exists('Sliced_Shared')) {

			if (isset($inv_id)) {
				$post_id = $this->__find_sliced_post_id($inv_id);

				$invoice_id = $inv_id;
			}
			else {
				// current post
				$post_id = get_the_id();

				if (function_exists('sliced_get_invoice_prefix'))
				$invoice_prefix = sliced_get_invoice_prefix();
		
				if (function_exists('sliced_get_invoice_sub_total'))
				$sub_total = sliced_get_invoice_sub_total();
		
				if (function_exists('sliced_get_invoice_number'))
				$invoice_number = sliced_get_invoice_number();
	
				$invoice_id = $invoice_prefix . $invoice_number;
			}

			// sliceinvoice
			$sub_total = 0;
			$invoice_prefix = 'INV';
			$invoice_number = '';
	
			$totals = array();

			if (function_exists('get_the_id'))
			$totals = Sliced_Shared::get_totals( $post_id );

			$invoice_info['amount'] = $totals['total_due'];

			$invoice_info['invoice_id'] = $invoice_id;
	
			$invoice_info['created'] = get_post_meta( $post_id, '_sliced_quote_created', true );

			$status = $this->_getInvoiceStatus($invoice_id);
			$invoice_info['status'] = $status;

			if ($status == 'paid') {
				// retrieve tx hash
				$tx_hash = get_post_meta( $post_id, '_primusmoney_tx');

				if (!empty($tx_hash))
				$invoice_info['tx_hash'] = $tx_hash[0];
	
			}
			else {
				// we are returning to the invoice page without having received a callback
				$transaction_hash = (isset($_GET['tx']) && !empty($_GET['tx']) ? $_GET['tx'] : NULL);

				if ($this->_isTransactionHashValid($transaction_hash, $invoice_id)) {
					// save transaction hash
					$this->_saveTansactionHash($invoice_id, $transaction_hash);

					// update invoice status
					$this->_setInvoiceStatus($invoice_id,'paid');

					$invoice_info['status'] = 'paid';
					$invoice_info['tx_hash'] = $transaction_hash;
				}
			}
		}

		return $invoice_info;
	}

	public function after_totals_table() {
		primus_debug_log('InvoicePOC::after_totals_table called');


		$payment_plugin = PrimusMoneyPayments::get_instance();
		$pocs_plugin = PrimusMoneyWordpressPOCs::get_instance();

		$invoice_params = array();

		// plugin info
		$current_version = PrimusMoneyWordpressPOCs::current_version();
		$exec_env = PrimusMoneyWordpressPOCs::execution_environment();

		$wp_script_root_url_dir = $pocs_plugin->plugin_root_dir();
		$wp_script_root_url_dir .= './assets/js/';

		$rest_end_point_url = $pocs_plugin->rest_api_url();
		$rest_payment_end_point_url = $payment_plugin->rest_api_url();


		$plugin_info = array(
			'version' => $current_version,
			'exec_env' => $exec_env,
			'scripts_dir_url' => $wp_script_root_url_dir,
			'rest_api_url' => $rest_end_point_url,
			'rest_payment_api_url' => $rest_payment_end_point_url,
		);


		// widget;
		$invoice_info = $this-> _getInvoiceInfo();

		$attr = array(
			'string_amount' => (isset($invoice_info['amount']) ? $invoice_info['amount'] : 0),
			'ccy_code' => 't-cUSD',
			'to_address' => '0x00c737fe578b40c8ade858c7bf4c4db368d7ac90',
		);

		// copy the standard payment params for primusmoney payment plugin
		$payment_params = $payment_plugin->payment_params($attr);
		$invoice_params = $payment_params;

		// plugin info
		$invoice_params['plugin'] = $plugin_info;
		
		// invoice
		$invoice_params['invoice'] = $invoice_info;
		$status = (isset($invoice_info['status']) ? $invoice_info['status'] : 'unknown');

		switch($status) {
			case 'Paid': 
				$invoice_params['widget']['pay_disabled'] = true;
				break;
			default:
				$invoice_params['widget']['widget'] = 'payment-qrcode';
				$invoice_params['widget']['callbackurl'] = $rest_payment_end_point_url  .'/callback?invoiceid=' . $invoice_info['invoice_id'];
				break;
		}

		// produce json string
		$invoice_json = json_encode($invoice_params);

		$html = '<div class="primusmoney-payment" id="primusmoney-payment">';

		// load payment plugin scripts
		$wp_script_url_dir = $payment_plugin->getJsDir();

		// plugin.js
		$wp_script_url = $wp_script_url_dir.'plugin.js';
		$html .= '<script src="'.$wp_script_url.'"></script>';
		
		// wordpress-widget-client.js
		$wp_script_url = $wp_script_url_dir.'wordpress-widget-client.js';
		$html .= '<script src="'.$wp_script_url.'"></script>';
		
		// payment-widget.js
		$wp_script_url = $wp_script_url_dir.'payment-widget.js';
		$html .= '<script src="'.$wp_script_url.'"></script>';
		

		$wp_script_url_dir = $this->getJsDir();

		// poc scripts
		
		// invoice.js
		$wp_script_url = $wp_script_url_dir.'invoice.js';
		$html .= '<script src="'.$wp_script_url.'"></script>';

		$html .= '<div class="primusmoney-invoice-widget" id="primusmoney-invoice-widget">';

		// initiate objects load
		$script_string = '<script type="text/javascript">';
		$script_string .= 'var _invoice_params = '.$invoice_json.';';

		$script_string .= 'PrimusMoneyInvoice.load(_invoice_params);';
		$script_string .= '</script>';

		$html .= $script_string;


		$html .= '</div>';

		$html .= '</div>';

		echo $html;

	}

	public function invoice_list($attrs = [], $content = null) {
		
		try {
			$content  = '<h3>Invoice list</h3>';

			$args = array(
				'post_type'     =>  'sliced_invoice',
				'post_status'    => 'publish',
				'posts_per_page' => -1,
				'fields'     	 => 'ids',
				'orderby' => 'date',
				'order' => 'DESC',
			);

			// load styles
			$wp_css_url_dir = $this->getCssDir();

			$wp_css_url = $wp_css_url_dir.'styles.css';

			$content .= '<link rel="stylesheet" href="'.$wp_css_url.'">';

			
			$post_ids = get_posts($args);

			$content .= '<table>';
			$content .= '<tbody>';

			$content .= '<tr>';
			$content .= '<th>ID</th>';
			$content .= '<th>Label</th>';
			$content .= '<th>Status</th>';
			$content .= '</tr>';


			foreach($post_ids as $post_id) {
				$sliced_number = get_post_meta( $post_id, '_sliced_number');
				$inv_id = (isset($sliced_number) && is_array($sliced_number) && isset($sliced_number[0]) ? $sliced_number[0] : NULL);
	
				if ($inv_id) {
					$invoice = get_post($post_id);

					$title = get_the_title($invoice);
					$permalink = get_permalink($invoice);
					$status = $this->_getInvoiceStatus($inv_id);
					
					$content  .= '<tr>';

					$content  .= '<td><a href="'.$permalink.'">'.$inv_id.'</td>';
					$content  .= '<td><a href="'.$permalink.'">'.$title.'</td>';

					switch($status) {
						case 'paid':
							$content .= '<td class="paid_status">Paid</td>';
							break;
						case 'unpaid':
							$content .= '<td class="unpaid_status">Not Paid</td>';
							break;

						default:
							$content .= '<td class="unpaid_status">'.$status.'</td>';
							break;
					}


					$content  .= '</tr>';

				}

			}

			$content .= '</tbody>';
			$content .= '</table>';

		}
		catch(Exception $e) {
			primus_debug_log('exception in InvoicePOC::invoice_list: '.$e->getMessage());

			$content = '<div>Exception has occured: '.$e->getMessage().'</div>';
			
			return $content;		
		}

		return $content;	
	}

}

// instantiation
$invoice_inst = InvoicePOC::get_instance();
