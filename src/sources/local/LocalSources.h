#pragma once

#include "sources/comics/ComicSource.h"
#include "sources/novels/NovelSource.h"

#include <memory>

namespace sc::local {

// The "On this device" sources, added to the registries next to the sites.
std::unique_ptr<comics::Source> createComicSource(const comics::SourceConfig &config);
std::unique_ptr<novels::Source> createNovelSource(const novels::SourceConfig &config);

} // namespace sc::local
