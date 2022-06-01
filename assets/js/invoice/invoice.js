if (typeof window.PrimusMoneyInvoice === "undefined") {
	class PrimusMoneyInvoice {
		constructor(invoice_params) {
			this.name = 'invoice_display';
			this.invoice_params = invoice_params;

			this.payment_widget = null;
			this.wp_widget = null;

		}

		async init() {
			console.log('WP - PrimusMoneyInvoice.init called');
			try {
				// wordpress widget
				window.addEventListener('wpwidgetclient_on_widget_loaded', this.onWidgetLoaded.bind(this), false);

				let widget_div = document.getElementById('primusmoney-invoice-widget');

				if (widget_div ) {
					// add div for payment widget
					let iframediv = document.createElement('div');
					iframediv.id = 'primusmoney-payment-widget';
					iframediv.classList.add('primusmoney-payment-widget');
					widget_div.appendChild(iframediv);

					// add div for version info
					let iversiondiv = document.createElement('div');
					iversiondiv.id = 'primusmoney-invoice-widget-versiondiv';
					iversiondiv.classList.add('primusmoney-invoice-widget-versiondiv');
					iversiondiv.innerHTML = (this.invoice_params && this.invoice_params.plugin ? 'pocs ver: ' + this.invoice_params.plugin.version : '');
					widget_div.appendChild(iversiondiv);
					
					

					// load widget
					let _widget_init = await this.loadWidget();

					return _widget_init;
				}	


			}
			catch(e) {
				console.log('WP - exception in PrimusMoneyInvoice.init: ' + e);
			}

		}

		async unloadWidget() {
			if (!this.wp_widget)
				return;

			let plugin = PrimusMoneyPlugin.getInstance();

			// remove listeners
			plugin.removeWindowEventListener('widget_on_pay', this.wp_widget.uuid);

			// unregister hook
			this.wp_widget.unregisterWidgetClientHook('widget_client_can_pay_async_hook', this.wp_widget.uuid);
			
			// tell widget to do its cleaning
			await this.wp_widget.onUnload();

			console.log('WP - wordress widget considered as unloaded: ' + this.wp_widget.uuid);

			plugin.dispatchEvent('wcorderreceived_on_unload_widget_done', {uuid: this.uuid, wp_widget_uuid: this.wp_widget.uuid});


			this.wp_widget = null;
		}


	   async loadWidget() {

			try {
				let plugin = PrimusMoneyPlugin.getInstance();
				
				if (this.wp_widget) {
					await this.unloadWidget()
					.catch(err => {
						console.log('WP - error in PrimusMoneyInvoice.loadWidget: ' + err);
						this.error('WP - error in PrimusMoneyInvoice.loadWidget: ' + err);
					});
				}

				// create payment widget
				this.payment_widget = new PrimusMoneyPayment(this.invoice_params);

				await this.payment_widget.init();

				this.wp_widget = this.payment_widget.wp_widget;

				plugin.dispatchEvent('wpinvoice_on_load_widget_done', {uuid: this.uuid, wp_widget_uuid: this.wp_widget.uuid});

				return (this.wp_widget ? true : false);
			}
			catch(e) {
				console.log('WP - exception in PrimusMoneyInvoice.loadWidget: ' + e);
				this.error('WP - exception in PrimusMoneyInvoice.loadWidget: ' + e);
			}

		}


		async onWidgetLoaded(ev) {
			console.log('WP - PrimusMoneyAuthorizationPage.onWidgetLoaded called for: ' + (this.wp_widget ? this.wp_widget.uuid : ''));

			try {
				let plugin = PrimusMoneyPlugin.getInstance();

				let data = ev.detail;
				let wp_widget_uuid = (data ? data.uuid : null);
				let wp_widget = PrimusMoneyWordpressWidget.getWidget(wp_widget_uuid);

				if (!wp_widget)
					throw Error('could not find widget with uuid: ' + wp_widget_uuid);

				if (this.wp_widget.uuid == wp_widget_uuid) {
					// invoice id
					let invoice_id = this._getInvoiceId();
					this.wp_widget.setInvoiceId(invoice_id);

					let tx_hash = this._getTransactionHash();

					if (tx_hash) {
						this._setTransactionHash(tx_hash);

					}
					else {
						let _wp_widget_type = this.wp_widget.widget_params.widget;

						switch(_wp_widget_type) {
							case 'pay':
								break;
							case 'payment-link':
								break;
							case 'payment-qrcode':
								// we keep looking if a transaction hash is returned to the server
								let _invoice = await this._fetchInvoice();

								let max_loops = 60; // 5 minutes
								let loop = 0;
								let lapse = 5000; // 5 s

								while(_invoice && !_invoice.tx_hash) {
									await plugin.sleep(lapse); // wait 5 s

									// ask order
									_invoice = await this._fetchInvoice();

									loop++;
									if (loop > max_loops) break;
								}

								if (_invoice && _invoice.tx_hash) {
									this._setTransactionHash(_invoice.tx_hash);

									let tx_info = await wp_widget.fetchTransactionInfo(_invoice.tx_hash);

									if ((tx_info && !tx_info.status_int)) {
										// we wait until transaction is found on the blockchain
										max_loops = 15; // 30 s
										loop = 0;
										lapse = 2000; // 2 s
										while(tx_info && !tx_info.status_int) {
											await plugin.sleep(lapse);

											// fetch tx_info
											tx_info = await wp_widget.fetchTransactionInfo(_order.tx_hash);

											loop++;
											if (loop > max_loops) break;
										}

										// provoke a refresh
										plugin.reloadPage();
										//this.wp_widget.refreshWidget();
									}

								}
								break;
							default:
								break;
						}						
					}

				}

	
			}
			catch(e) {
				console.log('WP - exception in PrimusMoneyAuthorizationPage.onWidgetLoaded: ' + e);
			}
		}

		async _fetchInvoice() {
			try {
				let plugin = PrimusMoneyPlugin.getInstance();

				let invoice_id = this._getInvoiceId();
	
				let route = '/invoice/read'
				let postdata = {invoice_id};

				return plugin.rest_post(route, postdata);
	
			}
			catch(e) {
				console.log('WP - exception in PrimusMoneyInvoice._fetchInvoice: ' + e);
				this.error('WP - exception in PrimusMoneyInvoice._fetchInvoice: ' + e);
			}
		}

		_getInvoiceId() {
			return (this.invoice_params.invoice ?this.invoice_params.invoice.invoice_id : null);
		}

		_getTransactionHash() {
			return (this.invoice_params.invoice ? this.invoice_params.invoice.tx_hash : null);
		}

		_setTransactionHash(tx_hash) {
			this.invoice_params.invoice.tx_hash = tx_hash;

			if (this.wp_widget)
			this.wp_widget.setTransactionHash(tx_hash);

		}

		// static
		static async load(invoice_params) {
			console.log('WP - PrimusMoneyInvoice.load called');
			try {
				let plugin_config = (invoice_params.plugin ? invoice_params.plugin : {});
				
				if (typeof window.PrimusMoneyPlugin  === "undefined") {
					// use jQuery to load ./plugin.js if not already done
					let url_dir = plugin_config.scripts_dir_url;
					await new Promise( (resolve, reject) => {
						window.jQuery.getScript(url_dir + 'plugin.js')
						.done(() => {
							resolve(true);
						})
						.fail( () => {
							reject('error loading script');
						})
					});
				}

				let plugin = PrimusMoneyPlugin.getInstance(plugin_config);
				
				if (typeof window.PrimusMoneyWordpressWidget  === "undefined") {
					// load ./wordpress-widget-client.js if not already done
					await await plugin.loadScript('wordpress-widget-client.js');
				}
				
				if (typeof window.PrimusMoneyPayment  === "undefined") {
					// load ./payment-widget.js if not already done
					await await plugin.loadScript('payment-widget.js');
				}
				
				// create invoice page object
				let invoice_page = new PrimusMoneyInvoice(invoice_params);

				// and initialize it (create the wordpress widget)
				let init = await invoice_page.init();
				
				if (init)
					return true;
			}
			catch(e) {
				console.log('WP - exception in PrimusMoneyInvoice.load: ' + e);
			}

		}
	}


	window.PrimusMoneyInvoice = PrimusMoneyInvoice;
}