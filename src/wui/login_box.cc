/*
 * Copyright (C) 2002-2019 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "wui/login_box.h"

#include "base/i18n.h"
#include "graphic/font_handler.h"
#include "network/crypto.h"
#include "network/internet_gaming.h"
#include "profile/profile.h"
#include "ui_basic/button.h"
#include "ui_basic/messagebox.h"

LoginBox::LoginBox(Panel& parent)
   : Window(&parent, "login_box", 0, 0, 500, 280, _("Online Game Settings")) {
	center_to_parent();

	int32_t margin = 10;

	ta_nickname = new UI::Textarea(this, margin, margin, _("Nickname:"));
	ta_password = new UI::Textarea(this, margin, 70, _("Password:"));
	eb_nickname = new UI::EditBox(this, 150, margin, 330, 20, 2, UI::PanelStyle::kWui);
	eb_password = new UI::EditBox(this, 150, 70, 330, 20, 2, UI::PanelStyle::kWui);

	cb_register = new UI::Checkbox(this, Vector2i(margin, 40), _("Log in to a registered account."),
	                               "", get_inner_w() - 2 * margin);

	register_account = new UI::MultilineTextarea(this, margin, 105, 470, 140, UI::PanelStyle::kWui,
			(boost::format(_("In order to use a registered "
				"account, you need an account on the widelands website. "
				"Please log in at %s and set an online "
				"gaming password on your profile page."))
				% "\n\nhttps://widelands.org/accounts/register/\n\n").str());

	loginbtn = new UI::Button(
	   this, "login",
	   UI::g_fh->fontset()->is_rtl() ? (get_inner_w() / 2 - 200) / 2 :
	                                   (get_inner_w() / 2 - 200) / 2 + get_inner_w() / 2,
	   get_inner_h() - 20 - margin, 200, 20, UI::ButtonStyle::kWuiPrimary, _("Save"));

	cancelbtn = new UI::Button(
	   this, "cancel",
	   UI::g_fh->fontset()->is_rtl() ? (get_inner_w() / 2 - 200) / 2 + get_inner_w() / 2 :
	                                   (get_inner_w() / 2 - 200) / 2,
	   loginbtn->get_y(), 200, 20, UI::ButtonStyle::kWuiSecondary, _("Cancel"));

	loginbtn->sigclicked.connect(boost::bind(&LoginBox::clicked_ok, boost::ref(*this)));
	cancelbtn->sigclicked.connect(boost::bind(&LoginBox::clicked_back, boost::ref(*this)));
	eb_nickname->changed.connect(boost::bind(&LoginBox::change_playername, this));
	cb_register->clickedto.connect(boost::bind(&LoginBox::clicked_register, this));

	Section& s = g_options.pull_section("global");
	eb_nickname->set_text(s.get_string("nickname", _("nobody")));
	cb_register->set_state(s.get_bool("registered", false));
	eb_password->set_password(true);

	if (registered()) {
		eb_password->set_text(s.get_string("password_sha1", ""));
		loginbtn->set_enabled(false);
	} else {
		eb_password->set_can_focus(false);
		ta_password->set_color(UI_FONT_CLR_DISABLED);
	}

	eb_nickname->focus();

}

/// think function of the UI (main loop)
void LoginBox::think() {
	verify_input();
}

/**
 * called, if "login" is pressed.
 */
void LoginBox::clicked_ok() {
	Section& s = g_options.pull_section("global");
	if (cb_register->get_state()) {
		if (check_password()) {
			s.set_string("nickname", eb_nickname->text());
			s.set_bool("registered", true);
			end_modal<UI::Panel::Returncodes>(UI::Panel::Returncodes::kOk);
		}
	} else {
		s.set_string("nickname", eb_nickname->text());
		s.set_bool("registered", false);
		s.set_string("password_sha1", "");
		end_modal<UI::Panel::Returncodes>(UI::Panel::Returncodes::kOk);
	}
}

/// Called if "cancel" was pressed
void LoginBox::clicked_back() {
	end_modal<UI::Panel::Returncodes>(UI::Panel::Returncodes::kBack);
}

/// Called when nickname was changed
void LoginBox::change_playername() {
	cb_register->set_state(false);
	eb_password->set_can_focus(false);
	eb_password->set_text("");
}

bool LoginBox::handle_key(bool down, SDL_Keysym code) {
	if (down) {
		switch (code.sym) {
		case SDLK_KP_ENTER:
		case SDLK_RETURN:
			clicked_ok();
			return true;
		case SDLK_ESCAPE:
			clicked_back();
			return true;
		default:
			break;  // not handled
		}
	}
	return UI::Panel::handle_key(down, code);
}

void LoginBox::clicked_register() {
	if (cb_register->get_state()) {
		ta_password->set_color(UI_FONT_CLR_DISABLED);
		eb_password->set_can_focus(false);
		eb_password->set_text("");
	} else {
		ta_password->set_color(UI_FONT_CLR_FG);
		eb_password->set_can_focus(true);
	}
}

void LoginBox::verify_input() {
	// Check if all neccessary input fields are valid
	loginbtn->set_enabled(true);
	eb_nickname->set_tooltip("");
	eb_password->set_tooltip("");
	eb_nickname->set_warning(false);

	if (eb_nickname->text().empty()) {
		eb_nickname->set_warning(true);
		eb_nickname->set_tooltip(_("Please enter a nickname!"));
		loginbtn->set_enabled(false);
	} else if (!InternetGaming::ref().valid_username(eb_nickname->text())) {
			eb_nickname->set_warning(true);
			eb_nickname->set_tooltip(_("Enter a valid nickname. This value may contain only "
													  "English letters, numbers, and @ . + - _ characters."));
			loginbtn->set_enabled(false);
	}

	if (eb_password->text().empty() && cb_register->get_state()) {
		eb_password->set_tooltip(_("Please enter your password!"));
		loginbtn->set_enabled(false);
		eb_password->focus();
	}

	Section& s = g_options.pull_section("global");
	if (eb_password->has_focus() && eb_password->text() == s.get_string("password_sha1", "")) {
		eb_password->set_text("");
	}

	if (cb_register->get_state() && eb_password->text() == s.get_string("password_sha1", "")) {
		loginbtn->set_enabled(false);
	}
}

/// Check password against metaserver
bool LoginBox::check_password() {
	// Try to connect to the metaserver
	Section& s = g_options.pull_section("global");
	const std::string& meta = s.get_string("metaserver", INTERNET_GAMING_METASERVER.c_str());
	uint32_t port = s.get_natural("metaserverport", kInternetGamingPort);
	std::string password = crypto::sha1(eb_password->text());
	InternetGaming::ref().login(get_nickname(), password, true, meta, port);

	if (!InternetGaming::ref().logged_in()) {
		// something went wrong -> show the error message
		// idealy it is about the wrong password
		ChatMessage msg = InternetGaming::ref().get_messages().back();
		UI::WLMessageBox wmb(this, _("Error!"), msg.msg, UI::WLMessageBox::MBoxType::kOk);
		wmb.run<UI::Panel::Returncodes>();
		InternetGaming::ref().reset();
		eb_password->set_text("");
		eb_password->focus();
		return false;
	}
	// NOTE: The password is only stored (in memory and on disk) and transmitted (over the network to the metaserver) as cryptographic hash.
	// This does NOT mean that the password is stored securely on the local disk.
	// While the password should be secure while transmitted to the metaserver (no-one can use the transmitted data to log in as the user) this is not the case for local storage.
	// The stored hash of the password makes it hard to look at the configuration file and figure out the plaintext password to, e.g., log in on the forum.
	// However, the stored hash can be copied to another system and used to log in as the user on the metaserver.
	// Further note: SHA-1 is considered broken and shouldn't be used anymore. But since the
	// passwords on the server are protected by SHA-1 we have to use it here, too
	s.set_string("password_sha1", password);
	InternetGaming::ref().logout();
	return true;
}
