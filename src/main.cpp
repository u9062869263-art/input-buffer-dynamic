#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace geode::prelude;

namespace ib {
	using Clock = std::chrono::steady_clock;
	using Stamp = Clock::time_point;

	// Jump  = cube, ball, robot, spider  -> buffer (keep input held a bit longer)
	// Hold  = ship, ufo, swing           -> min hold (stretch short taps)
	// Wave  = wave                       -> min hold + min gap
	enum class Kind { Jump, Hold, Wave };

	struct Channel {
		bool logicalDown = false;   // what the game currently believes
		bool physicalDown = false;  // what your finger is doing
		bool pendingPress = false;
		bool pendingRelease = false;
		Stamp pressedAt{};
		Stamp releasedAt{};
		Stamp pressDue{};
		Stamp releaseDue{};
		Kind kind = Kind::Jump;
	};

	static Channel channels[2]; // 0 = player 1, 1 = player 2
	static bool passthrough = false;

	static bool enabled() {
		return Mod::get()->getSettingValue<bool>("enabled");
	}

	static double num(char const* key) {
		return Mod::get()->getSettingValue<double>(key);
	}

	static Kind kindFor(PlayerObject* p) {
		if (!p) return Kind::Jump;
		if (p->m_isDart) return Kind::Wave;
		if (p->m_isShip || p->m_isBird || p->m_isSwing) return Kind::Hold;
		return Kind::Jump;
	}

	static Stamp after(Stamp t, double ms) {
		return t + std::chrono::duration_cast<Clock::duration>(
			std::chrono::duration<double, std::milli>(ms)
		);
	}

	static double snap(double ms) {
		if (ms <= 0.0 || !Mod::get()->getSettingValue<bool>("grid-snap")) return ms;
		double tps = static_cast<double>(Mod::get()->getSettingValue<int64_t>("grid-tps"));
		double tick = 1000.0 / std::max(1.0, tps);
		return std::ceil(ms / tick - 1e-9) * tick;
	}

	static Stamp releaseTime(Channel const& c, Stamp now) {
		double extra = 0.0;
		double minHold = 0.0;
		switch (c.kind) {
			case Kind::Jump: extra = num("jump-buffer-ms"); break;
			case Kind::Hold: minHold = num("hold-min-ms"); break;
			case Kind::Wave: minHold = num("wave-min-hold-ms"); break;
		}
		return std::max(after(now, snap(extra)), after(c.pressedAt, snap(minHold)));
	}
}

class $modify(IBBaseLayer, GJBaseGameLayer) {
	void sendButton(bool down, bool p1) {
		ib::passthrough = true;
		GJBaseGameLayer::handleButton(down, 1, p1);
		ib::passthrough = false;
	}

	void flushBuffered() {
		auto now = ib::Clock::now();
		for (int i = 0; i < 2; ++i) {
			auto& c = ib::channels[i];
			bool p1 = (i == 0);

			if (c.pendingPress && now >= c.pressDue) {
				c.pendingPress = false;
				c.logicalDown = true;
				c.pressedAt = now;
				this->sendButton(true, p1);
				if (!c.physicalDown) {
					// finger already lifted while the press was delayed
					c.pendingRelease = true;
					c.releaseDue = ib::releaseTime(c, now);
				}
			}

			if (c.pendingRelease && now >= c.releaseDue) {
				c.pendingRelease = false;
				c.logicalDown = false;
				c.releasedAt = now;
				this->sendButton(false, p1);
			}
		}
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		if (ib::passthrough || button != 1 || !ib::enabled()) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		auto& c = ib::channels[isPlayer1 ? 0 : 1];
		auto now = ib::Clock::now();
		c.kind = ib::kindFor(isPlayer1 ? m_player1 : m_player2);

		if (down) {
			c.physicalDown = true;

			// Still held from a stretched/buffered tap: just cancel the pending release.
			if (c.pendingRelease) {
				c.pendingRelease = false;
				return;
			}
			if (c.logicalDown || c.pendingPress) return;

			if (c.kind == ib::Kind::Wave) {
				auto earliest = ib::after(c.releasedAt, ib::snap(ib::num("wave-min-gap-ms")));
				if (now < earliest) {
					c.pendingPress = true;
					c.pressDue = earliest;
					return;
				}
			}

			c.logicalDown = true;
			c.pressedAt = now;
			this->sendButton(true, isPlayer1);
		}
		else {
			c.physicalDown = false;
			if (!c.logicalDown) return; // nothing to release (a delayed press handles itself)

			c.pendingRelease = true;
			c.releaseDue = ib::releaseTime(c, now);
			if (c.releaseDue <= now) this->flushBuffered();
		}
	}

	void update(float dt) {
		if (ib::enabled()) this->flushBuffered();
		GJBaseGameLayer::update(dt);
	}
};

class $modify(IBPlayLayer, PlayLayer) {
	void resetLevel() {
		for (auto& c : ib::channels) c = ib::Channel{};
		PlayLayer::resetLevel();
	}
};

class $modify(IBKeyboard, CCKeyboardDispatcher) {
	bool handleTab(enumKeyCodes key, bool down, bool repeat) {
		if (key != KEY_Tab || !down || repeat) return false;
		auto scene = CCDirector::get()->getRunningScene();
		if (!scene || scene->getChildByType<FLAlertLayer>(0)) return false;
		openSettingsPopup(Mod::get(), false);
		return true;
	}

#if GEODE_COMP_GD_VERSION >= 22074
	bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isKeyRepeat, double timestamp) {
		if (this->handleTab(key, down, isKeyRepeat)) return true;
		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isKeyRepeat, timestamp);
	}
#else
	bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isKeyRepeat) {
		if (this->handleTab(key, down, isKeyRepeat)) return true;
		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isKeyRepeat);
	}
#endif
};
