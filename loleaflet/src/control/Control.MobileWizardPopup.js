/* -*- js-indent-level: 8 -*- */
/*
 * L.Control.MobileWizardPopup - shows a popup with content styled like in mobile-wizard.
                                 it will be used to show touch-device friendly UI in popups
								 on devices with smaller screens which are not smartphones
								 (desktop and tablets). Example of usage: show comments preview
								 when browser has reduced size.
 */

/* global app $ */
L.Control.MobileWizardPopup = L.Control.extend({
	_builder: null,
	_overlay: null,
	_inMainMenu: true,
	_isActive: false,
	_inBuilding: false,
	_currentDepth: 0,
	_mainTitle: '',
	_customTitle: false,
	_isTabMode: false,
	_currentPath: [],
	_tabs: [],
	_currentScrollPosition: 0,

	initialize: function (options) {
		L.setOptions(this, options);
	},

	onAdd: function (map) {
		this.map = map;

		map.on('mobilewizardpopup', this._onMobileWizardPopup, this);
		map.on('mobilewizardpopupresize', this._onResizeRequest, this);
		map.on('mobilewizardpopupclose', this._hideWizard, this);
		window.addEventListener('resize', this._hideWizard.bind(this));
	},

	removeContainer: function () {
		L.DomUtil.remove(this._overlay);
		L.DomUtil.remove(this._container);
	},

	onRemove: function() {
		this.removeContainer();

		this.map.off('mobilewizardpopup', this._onMobileWizardPopup, this);
		this.map.off('mobilewizardpopupresize', this._onResizeRequest, this);
		this.map.off('mobilewizardpopupclose', this._hideWizard, this);
		window.removeEventListener('resize', this._hideWizard.bind(this));
	},

	_hideWizard: function () {
		this.removeContainer();

		app.sectionContainer.getSectionWithName(L.CSections.CommentList.name).removeHighlighters();
		app.sectionContainer.getSectionWithName(L.CSections.CommentList.name).layout();
	},

	_setTitle: function(title) {
		this.title.innerText = title;
	},

	_setCustomTitle: function(title) {
		this.title.innerHTML = title;
	},

	goLevelDown: function(contentToShow, options) {
		var animate = (options && options.animate != undefined) ? options.animate : true;

		if (!this._isTabMode || this._currentDepth > 0)
			L.DomUtil.removeClass(this.backButton, 'close-button');

		if (this._isTabMode && this._currentDepth > 0) {
			this.titlebar.show();
		}

		var nodesToHide = $(contentToShow).siblings().not('.mobile-wizard-scroll-indicator');

		var parent = $(contentToShow).parent();
		if (parent.hasClass('toolbox'))
			nodesToHide = nodesToHide.add(parent.siblings().not('.mobile-wizard-scroll-indicator'));

		var duration = 10;
		if (animate) {
			nodesToHide.hide('slide', { direction: 'left' }, duration);
			// be sure all is hidden, sometimes jQuery doesn't work here ...
			// restoreStyle is called in some jQuery cleanup what causes showing nodes again
			setTimeout(function() { nodesToHide.hide(); }, duration + 5);
		}
		else
			nodesToHide.hide();

		$(contentToShow).children('.ui-header').hide();

		if (animate)
			$(contentToShow).children('.ui-content').first().show('slide', { direction: 'right' }, 'fast');
		else
			$(contentToShow).children('.ui-content').first().show();

		this._currentDepth++;
		if (!this._inBuilding)
			history.pushState({context: 'mobile-wizard', level: this._currentDepth}, 'mobile-wizard-level-' + this._currentDepth);

		var title = $(contentToShow).children('.ui-content').get(0).title;

		if (this._customTitle)
			this._setCustomTitle(this._customTitle);
		else
			this._setTitle(title);

		this._inMainMenu = false;

		this._currentPath.push(title);
	},

	goLevelUp: function() {
		this._currentPath.pop();
		if (this._inMainMenu || (this._isTabMode && this._currentDepth == 1)) {
			this._hideWizard();
			this._currentDepth = 0;
		} else {
			this._currentDepth--;

			var parent = $('.ui-content.mobile-wizard:visible');
			if (this._currentDepth > 0 && parent)
				this._customTitle ? this._setCustomTitle(parent.get(0).customTitleBar) : this._setTitle(parent.get(0).title);
			else
				this._customTitle ? this._setCustomTitle(this._customTitle) : this._setTitle(this._mainTitle);

			var currentNode = $('.ui-explorable-entry.level-' + this._currentDepth + '.mobile-wizard:visible');
			var headers = currentNode.siblings();
			var currentHeader = currentNode.children('.ui-header');
			headers = headers.add(currentHeader);

			var parent = currentNode.parent();
			if (parent.hasClass('toolbox'))
				headers = headers.add(parent.siblings());

			headers = headers.not('.hidden');

			$('.ui-content.level-' + this._currentDepth + '.mobile-wizard:visible').hide();
			headers.show('slide', { direction: 'left' }, 'fast');

			if (this._currentDepth == 0 || (this._isTabMode && this._currentDepth == 1)) {
				this._inMainMenu = true;
				L.DomUtil.addClass(this.backButton, 'close-button');
				if (this._isTabMode) {
					this.titlebar.hide();
				}
			}

			app.sectionContainer.getSectionWithName(L.CSections.CommentList.name).removeHighlighters();
		}
	},

	_onResizeRequest: function() {
		if (!this._container)
			return;

		var posX = this._container.getBoundingClientRect().right;
		var posY = this._container.getBoundingClientRect().bottom;

		if (posX > window.innerWidth) {
			var diff = posX - window.innerWidth + 20;
			posX = this._container.getBoundingClientRect().left - diff;
			this._container.style.marginLeft = posX + 'px';
		}

		if (posY > window.innerHeight) {
			diff = posY - window.innerHeight + 20;
			posY = this._container.getBoundingClientRect().top - diff;
			this._container.style.marginTop = posY + 'px';
		}
	},

	_onMobileWizardPopup: function(data) {
		this.removeContainer();

		var callback = data.callback;
		data = data.data;
		if (data) {
			var that = this;

			this._hideWizard();

			this._currentDepth = 0;
			this._inMainMenu = true;
			this._isTabMode = false;
			this._currentPath = [];
			this._tabs = [];

			this._container = L.DomUtil.create('div', 'jsdialog-container ui-dialog ui-widget-content lokdialog_container', document.body);
			this._container.id = 'mobile-wizard-popup';
			this._container.style.visibility = 'hidden';
			if (data.collapsed && (data.collapsed === 'true' || data.collapsed === true))
				L.DomUtil.addClass(this._container, 'collapsed');

			L.DomUtil.addClass(this._container, 'modalpopup');

			this.titlebar = L.DomUtil.create('table', 'mobile-wizard-titlebar', this._container);
			this.titlebar.style.width = '100%';
			this.titlebar.style.top = '0px';
			var tr = L.DomUtil.create('tr', 'mobile-wizard-titlebar', this.titlebar);
			this.backButton = L.DomUtil.create('td', 'mobile-wizard-back', tr);
			L.DomUtil.addClass(this.backButton, 'close-button');
			this.backButton.onclick = function() { that.goLevelUp(); };
			this.title = L.DomUtil.create('td', 'mobile-wizard-title ui-widget', tr);

			var content = L.DomUtil.create('div', 'lokdialog ui-dialog-content', this._container);
			this.content = content;
			this.content.style.maxHeight = (that.map._container.getBoundingClientRect().height - 50) + 'px';

			var builder = new L.control.jsDialogBuilder({windowId: data.id, mobileWizard: this, map: this.map, cssClass: 'jsdialog'});

			this._overlay = L.DomUtil.create('div', builder.options.cssClass + ' jsdialog-overlay cancellable', document.body);
			this._overlay.onclick = function () { that._hideWizard(); };

			this._builder = L.control.mobileWizardBuilder({windowId: data.id, mobileWizard: this, map: this.map, cssClass: 'mobile-wizard', callback: callback});
			this._builder.build(content, [data]);

			if (!this.content.querySelector('.ui-explorable-entry'))
				this.titlebar.style.display = 'none';

			var isCommentWizard = data.children && data.children.length && data.children[0].type == 'comment';

			var posX = 0;
			var posY = 0;
			var setupPosition = function () {
				if (data.popupParent && L.DomUtil.get(data.popupParent)) {
					var parent = L.DomUtil.get(data.popupParent);
					posX = parent.getBoundingClientRect().left - content.clientWidth + 10;
					posY = parent.getBoundingClientRect().bottom - 10;

					if (posX < 0)
						posX = 20;
					if (posY + content.clientHeight > window.innerHeight) {
						posY -= posY + content.clientHeight - window.innerHeight + 20;

						if (isCommentWizard) {
							(new L.PosAnimation()).run(parent,
								{
									x: parent.getBoundingClientRect().left,
									y: posY - that.map._container.getBoundingClientRect().top - 40
								});
						}
					}
				} else {
					posX = window.innerWidth/2 - this._container.offsetWidth/2;
					posY = window.innerHeight/2 - this._container.offsetHeight/2;
				}
			};

			setupPosition();

			this._container.style.marginLeft = posX + 'px';
			this._container.style.marginTop = posY + 'px';

			setTimeout(function () {
				setupPosition();
				that._container.style.marginLeft = posX + 'px';
				that._container.style.marginTop = posY + 'px';
				that._container.style.visibility = '';
			}, 200);
		}
	},
});

L.control.mobileWizardPopup = function (options) {
	return new L.Control.MobileWizardPopup(options);
};