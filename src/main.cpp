#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/binding/MenuLayer.hpp>
#include <Geode/cocos/label_nodes/CCLabelBMFont.h>
#include <Geode/cocos/layers_scenes_transitions_nodes/CCLayer.h>
#include <Geode/cocos/layers_scenes_transitions_nodes/CCScene.h>
#include <Geode/cocos/menus/CCMenu.h>
#include <Geode/loader/Mod.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr char FAVORITES_KEY[] = "favorite-levels";
    constexpr char LAST_BACKUP_KEY[] = "last-backup-epoch";
    constexpr int PAGE_SIZE = 8;

    struct FavoriteLevelEntry {
        int levelID = 0;
        std::string name;
        std::string creator;
        int stars = 0;
    };

    std::vector<FavoriteLevelEntry> g_favoritesCache;
    bool g_favoritesLoaded = false;

    template <>
    struct matjson::Serialize<FavoriteLevelEntry> {
        static matjson::Value toJson(FavoriteLevelEntry const& value) {
            auto root = matjson::makeObject();
            root["level-id"] = value.levelID;
            root["name"] = value.name;
            root["creator"] = value.creator;
            root["stars"] = value.stars;
            return root;
        }

        static Result<FavoriteLevelEntry> fromJson(matjson::Value const& value) {
            FavoriteLevelEntry entry;

            GEODE_UNWRAP_INTO(entry.levelID, value["level-id"].as<int>());

            if (value.contains("name")) {
                GEODE_UNWRAP_INTO(entry.name, value["name"].as<std::string>());
            }

            if (value.contains("creator")) {
                GEODE_UNWRAP_INTO(entry.creator, value["creator"].as<std::string>());
            }

            if (value.contains("stars")) {
                GEODE_UNWRAP_INTO(entry.stars, value["stars"].as<int>());
            }

            return Ok(entry);
        }
    };

    template <class T>
    auto unwrapField(T const& value) {
        if constexpr (requires { value.value(); }) {
            return value.value();
        } else {
            return value;
        }
    }

    std::vector<FavoriteLevelEntry> const& getFavorites() {
        if (!g_favoritesLoaded) {
            g_favoritesCache = Mod::get()->getSavedValue<std::vector<FavoriteLevelEntry>>(FAVORITES_KEY, {});
            g_favoritesLoaded = true;
        }
        return g_favoritesCache;
    }

    void setFavorites(std::vector<FavoriteLevelEntry> const& favorites) {
        g_favoritesCache = favorites;
        g_favoritesLoaded = true;
        Mod::get()->setSavedValue(FAVORITES_KEY, favorites);
    }

    int getLevelID(GJGameLevel* level) {
        if (!level) {
            return 0;
        }
        return static_cast<int>(unwrapField(level->m_levelID));
    }

    int getLevelStars(GJGameLevel* level) {
        if (!level) {
            return 0;
        }
        return static_cast<int>(unwrapField(level->m_stars));
    }

    std::string getLevelName(GJGameLevel* level) {
        if (!level) {
            return "Unknown";
        }
        return std::string(level->m_levelName);
    }

    std::string getCreatorName(GJGameLevel* level) {
        if (!level) {
            return "";
        }
        return std::string(level->m_creatorName);
    }

    FavoriteLevelEntry makeEntry(GJGameLevel* level) {
        FavoriteLevelEntry entry;
        entry.levelID = getLevelID(level);
        entry.name = getLevelName(level);
        entry.creator = getCreatorName(level);
        entry.stars = getLevelStars(level);
        return entry;
    }

    bool isFavorite(int levelID) {
        auto const& favorites = getFavorites();
        return std::ranges::any_of(favorites, [levelID](auto const& item) {
            return item.levelID == levelID;
        });
    }

    std::string makeBackupFileName() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::tm localTime {};
#ifdef GEODE_IS_WINDOWS
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif

        char buffer[64] = {};
        std::strftime(buffer, sizeof(buffer), "favorites-%Y%m%d-%H%M%S.json", &localTime);
        return buffer;
    }

    void pruneBackups(std::filesystem::path const& backupDir, std::size_t keepCount) {
        if (!std::filesystem::exists(backupDir)) {
            return;
        }

        std::vector<std::filesystem::directory_entry> files;
        try {
            for (auto const& entry : std::filesystem::directory_iterator(backupDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    files.push_back(entry);
                }
            }
        } catch (std::filesystem::filesystem_error const&) {
            return;
        }

        std::sort(files.begin(), files.end(), [](auto const& a, auto const& b) {
            return a.last_write_time() > b.last_write_time();
        });

        if (files.size() <= keepCount) {
            return;
        }

        for (auto it = files.begin() + static_cast<std::ptrdiff_t>(keepCount); it != files.end(); ++it) {
            std::error_code ec;
            std::filesystem::remove(it->path(), ec);
        }
    }

    void writeBackupFile() {
        auto backupDir = Mod::get()->getSaveDir() / "backups";
        std::filesystem::create_directories(backupDir);

        auto root = matjson::makeObject();
        root["created-at"] = static_cast<int64_t>(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
        );
        root["favorites"] = getFavorites();

        auto backupPath = backupDir / makeBackupFileName();
        std::ofstream stream(backupPath, std::ios::binary | std::ios::trunc);
        stream << root.dump(matjson::NO_INDENTATION);
        stream.close();

        auto keepCount = static_cast<std::size_t>(
            std::max<int64_t>(1, Mod::get()->getSettingValue<int64_t>("auto-backup-keep-count"))
        );
        pruneBackups(backupDir, keepCount);
        Mod::get()->setSavedValue(
            LAST_BACKUP_KEY,
            static_cast<int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))
        );
    }

    void backupFavoritesIfDue(bool force = false) {
        if (!Mod::get()->getSettingValue<bool>("auto-backup-enabled")) {
            return;
        }

        auto now = static_cast<int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        auto lastBackup = Mod::get()->getSavedValue<int64_t>(LAST_BACKUP_KEY, 0);
        auto intervalMinutes = std::max<int64_t>(
            1,
            Mod::get()->getSettingValue<int64_t>("auto-backup-interval-minutes")
        );

        if (force || now - lastBackup >= intervalMinutes * 60) {
            writeBackupFile();
        }
    }

    void addFavorite(GJGameLevel* level) {
        if (!level) {
            return;
        }

        auto favorites = getFavorites();
        auto levelID = getLevelID(level);
        auto found = std::ranges::find_if(favorites, [levelID](auto const& item) {
            return item.levelID == levelID;
        });

        if (found == favorites.end()) {
            favorites.push_back(makeEntry(level));
        } else {
            *found = makeEntry(level);
        }

        setFavorites(favorites);
        backupFavoritesIfDue();
    }

    void removeFavorite(int levelID) {
        auto favorites = getFavorites();
        std::erase_if(favorites, [levelID](auto const& item) {
            return item.levelID == levelID;
        });
        setFavorites(favorites);
        backupFavoritesIfDue();
    }

    std::string makeCaption(FavoriteLevelEntry const& entry) {
        return entry.name + " [" + std::to_string(entry.stars) + "*]";
    }

    std::string makeMeta(FavoriteLevelEntry const& entry) {
        auto creator = entry.creator.empty() ? std::string("Unknown Creator") : entry.creator;
        return "ID " + std::to_string(entry.levelID) + " | by " + creator;
    }

    std::string makePageText(int page, int totalPages, std::size_t count) {
        return "Page " + std::to_string(page) + "/" + std::to_string(totalPages) +
            " | " + std::to_string(count) + " favorites";
    }

    class FavoritesLayer : public CCLayer {
    protected:
        std::vector<FavoriteLevelEntry> m_entries;
        CCMenu* m_listMenu = nullptr;
        CCLabelBMFont* m_pageLabel = nullptr;
        CCLabelBMFont* m_statusLabel = nullptr;
        CCLabelBMFont* m_emptyLabel = nullptr;
        int m_page = 0;

        void setStatus(std::string const& text) {
            if (!m_statusLabel) {
                return;
            }
            m_statusLabel->setString(text.c_str());
        }

        void refreshPage() {
            if (!m_listMenu) {
                return;
            }

            m_listMenu->removeAllChildrenWithCleanup(true);
            m_entries = getFavorites();

            auto totalPages = std::max(1, static_cast<int>((m_entries.size() + PAGE_SIZE - 1) / PAGE_SIZE));
            m_page = std::clamp(m_page, 0, totalPages - 1);

            if (m_pageLabel) {
                auto text = makePageText(m_page + 1, totalPages, m_entries.size());
                m_pageLabel->setString(text.c_str());
            }

            if (m_emptyLabel) {
                m_emptyLabel->setVisible(m_entries.empty());
            }

            auto size = CCDirector::sharedDirector()->getWinSize();
            auto startIndex = m_page * PAGE_SIZE;
            auto endIndex = std::min(static_cast<int>(m_entries.size()), startIndex + PAGE_SIZE);
            float y = size.height - 95.0f;

            for (int index = startIndex; index < endIndex; ++index) {
                auto const& entry = m_entries.at(index);

                auto levelButton = CCMenuItemSpriteExtra::create(
                    ButtonSprite::create(makeCaption(entry).c_str()),
                    this,
                    menu_selector(FavoritesLayer::onOpenLevel)
                );
                levelButton->setTag(entry.levelID);
                levelButton->setPosition({ size.width / 2 - 20.0f, y });
                m_listMenu->addChild(levelButton);

                auto removeButton = CCMenuItemSpriteExtra::create(
                    ButtonSprite::create("Remove"),
                    this,
                    menu_selector(FavoritesLayer::onRemoveLevel)
                );
                removeButton->setTag(entry.levelID);
                removeButton->setScale(0.65f);
                removeButton->setPosition({ size.width / 2 + 170.0f, y });
                m_listMenu->addChild(removeButton);

                auto meta = CCLabelBMFont::create(makeMeta(entry).c_str(), "goldFont.fnt");
                meta->setScale(0.42f);
                meta->setPosition({ size.width / 2 - 20.0f, y - 18.0f });
                m_listMenu->addChild(meta);

                y -= 42.0f;
            }
        }

    public:
        static FavoritesLayer* create() {
            auto ret = new FavoritesLayer();
            if (ret && ret->init()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        static CCScene* scene() {
            auto scene = CCScene::create();
            scene->addChild(FavoritesLayer::create());
            return scene;
        }

        void onBack(CCObject*) {
            CCDirector::sharedDirector()->replaceScene(MenuLayer::scene(false));
        }

        void onPrevPage(CCObject*) {
            if (m_page > 0) {
                --m_page;
                refreshPage();
            }
        }

        void onNextPage(CCObject*) {
            auto totalPages = std::max(1, static_cast<int>((m_entries.size() + PAGE_SIZE - 1) / PAGE_SIZE));
            if (m_page + 1 < totalPages) {
                ++m_page;
                refreshPage();
            }
        }

        void onOpenLevel(CCObject* sender) {
            auto node = static_cast<CCNode*>(sender);
            auto levelID = node->getTag();
            auto level = GameLevelManager::sharedState()->getSavedLevel(levelID);

            if (!level) {
                setStatus("Level is not cached locally yet.");
                return;
            }

            setStatus("Opened cached level.");
            CCDirector::sharedDirector()->pushScene(LevelInfoLayer::scene(level, false));
        }

        void onRemoveLevel(CCObject* sender) {
            auto node = static_cast<CCNode*>(sender);
            removeFavorite(node->getTag());
            setStatus("Level removed from favorites.");
            refreshPage();
        }

        bool init() override {
            if (!CCLayer::init()) {
                return false;
            }

            auto size = CCDirector::sharedDirector()->getWinSize();

            if (auto background = CCSprite::create("GJ_gradientBG.png")) {
                background->setPosition({ size.width / 2, size.height / 2 });
                background->setColor({ 0, 102, 255 });
                background->setScaleX(size.width / background->getContentSize().width);
                background->setScaleY(size.height / background->getContentSize().height);
                this->addChild(background, -10);
            } else {
                auto fallback = CCLayerColor::create(ccc4(10, 25, 45, 255));
                this->addChild(fallback, -10);
            }

            auto title = CCLabelBMFont::create("Favorite Levels", "goldFont.fnt");
            title->setPosition({ size.width / 2, size.height - 28.0f });
            this->addChild(title);

            m_pageLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_pageLabel->setScale(0.5f);
            m_pageLabel->setPosition({ size.width / 2, size.height - 52.0f });
            this->addChild(m_pageLabel);

            m_emptyLabel = CCLabelBMFont::create("No favorite levels yet.", "bigFont.fnt");
            m_emptyLabel->setScale(0.5f);
            m_emptyLabel->setPosition({ size.width / 2, size.height / 2 });
            this->addChild(m_emptyLabel);

            m_statusLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_statusLabel->setScale(0.42f);
            m_statusLabel->setPosition({ size.width / 2, 15.0f });
            this->addChild(m_statusLabel);

            m_listMenu = CCMenu::create();
            m_listMenu->setPosition({ 0.0f, 0.0f });
            this->addChild(m_listMenu);

            auto pageMenu = CCMenu::create();
            pageMenu->setPosition({ size.width / 2, 36.0f });
            this->addChild(pageMenu);

            auto prevButton = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("<"),
                this,
                menu_selector(FavoritesLayer::onPrevPage)
            );
            prevButton->setPosition({ -50.0f, 0.0f });
            pageMenu->addChild(prevButton);

            auto nextButton = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(">"),
                this,
                menu_selector(FavoritesLayer::onNextPage)
            );
            nextButton->setPosition({ 50.0f, 0.0f });
            pageMenu->addChild(nextButton);

            auto backMenu = CCMenu::create();
            backMenu->setPosition({ 56.0f, size.height - 30.0f });
            this->addChild(backMenu);

            auto backButton = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Back"),
                this,
                menu_selector(FavoritesLayer::onBack)
            );
            backMenu->addChild(backButton);

            refreshPage();
            return true;
        }
    };
}

class $modify(FavoriteLevelsMenuLayer, MenuLayer) {
    void onFavoriteLevels(CCObject*) {
        backupFavoritesIfDue();
        CCDirector::sharedDirector()->replaceScene(FavoritesLayer::scene());
    }

    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        backupFavoritesIfDue();

        auto size = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 78.0f, size.height - 78.0f });
        this->addChild(menu, 20);

        auto button = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Favorites *"),
            this,
            menu_selector(FavoriteLevelsMenuLayer::onFavoriteLevels)
        );
        button->setScale(0.62f);
        menu->addChild(button);

        return true;
    }
};

class $modify(FavoriteLevelsInfoLayer, LevelInfoLayer) {
    struct Fields {
        ButtonSprite* favoriteButtonSprite = nullptr;
    };

    void updateFavoriteButton() {
        if (!m_fields->favoriteButtonSprite || !m_level) {
            return;
        }

        auto favorited = isFavorite(getLevelID(m_level));
        m_fields->favoriteButtonSprite->setString(
            favorited ? "Remove Favorite" : "Add to Favorite"
        );
    }

    void onToggleFavorite(CCObject*) {
        if (!m_level) {
            return;
        }

        auto levelID = getLevelID(m_level);
        if (isFavorite(levelID)) {
            removeFavorite(levelID);
        } else {
            addFavorite(m_level);
        }

        updateFavoriteButton();
    }

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) {
            return false;
        }

        backupFavoritesIfDue();

        auto size = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({ size.width - 95.0f, size.height - 92.0f });
        this->addChild(menu, 30);

        m_fields->favoriteButtonSprite = ButtonSprite::create("Add to Favorite");
        auto favoriteButton = CCMenuItemSpriteExtra::create(
            m_fields->favoriteButtonSprite,
            this,
            menu_selector(FavoriteLevelsInfoLayer::onToggleFavorite)
        );
        favoriteButton->setScale(0.62f);
        menu->addChild(favoriteButton);

        updateFavoriteButton();
        return true;
    }
};
