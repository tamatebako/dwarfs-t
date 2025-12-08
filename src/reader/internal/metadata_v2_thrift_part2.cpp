
std::string metadata_v2_data::serialize_as_json(bool simple) const {
  using namespace apache::thrift;
  std::string json;
  if (simple) {
    SimpleJSONSerializer::serialize(unpack_metadata(), &json);
  } else {
    JSONSerializer::serialize(unpack_metadata(), &json);
  }
  return json;
}

nlohmann::json metadata_v2_data::as_json(dir_entry_view const& entry) const {
  nlohmann::json arr = nlohmann::json::array();

  if (entry.inode().is_directory()) {
    auto dir = make_directory_view(entry.inode());
    auto range = dir.entry_range();

    for (auto i : range) {
      arr.push_back(as_json(make_dir_entry_view(i, entry.raw().self_index())));
    }
  }

  return arr;
}

nlohmann::json metadata_v2_data::as_json() const {
  vfs_stat stbuf;
  statvfs(&stbuf);

  nlohmann::json obj{
      {"statvfs",
       {{"f_bsize", stbuf.bsize},
        {"f_files", stbuf.files},
        {"f_blocks", stbuf.blocks}}},
      {"root", as_json(root_)},
  };

  return obj;
}

nlohmann::json
metadata_v2_data::info_as_json(fsinfo_options const& opts,
                               filesystem_info const* fsinfo) const {
  nlohmann::json info;
  vfs_stat stbuf;
  statvfs(&stbuf);

  if (auto version = meta_.dwarfs_version()) {
    info["created_by"] = version.value();
  }

  if (auto ts = meta_.create_timestamp()) {
    info["created_on"] = fmt::format("{:%FT%T}", safe_localtime(ts.value()));
  }

  if (detected_format_) {
    info["metadata_format"] = std::string(metadata::serialization::get_format_name(*detected_format_));
  }

  if (auto features = meta_.features(); features) {
    info["features"] = *features;
  }

  if (opts.features.has(fsinfo_feature::metadata_summary)) {
    info["block_size"] = meta_.block_size();
    if (fsinfo) {
      info["block_count"] = fsinfo->block_count;
    }
    info["inode_count"] = stbuf.files;
    if (auto ps = meta_.preferred_path_separator()) {
      info["preferred_path_separator"] = std::string(1, static_cast<char>(*ps));
    }
    info["original_filesystem_size"] = meta_.total_fs_size();
    if (auto const allocated = meta_.total_allocated_fs_size()) {
      info["original_allocated_filesystem_size"] = *allocated;
    }
    if (fsinfo) {
      info["compressed_block_size"] = fsinfo->compressed_block_size;
      if (!fsinfo->uncompressed_block_size_is_estimate) {
        info["uncompressed_block_size"] = fsinfo->uncompressed_block_size;
      }
      info["compressed_metadata_size"] = fsinfo->compressed_metadata_size;
      if (!fsinfo->uncompressed_metadata_size_is_estimate) {
        info["uncompressed_metadata_size"] = fsinfo->uncompressed_metadata_size;
      }
    }

    if (auto opt = meta_.options()) {
      nlohmann::json options;

      push_back_if_enabled add_to_options(options);
      parse_fs_options(*opt, add_to_options);
      parse_string_table_options(meta_, add_to_options);

      info["options"] = std::move(options);
    }

    timeres_handler_.add_time_resolution_to(info);

    if (meta_.block_categories()) {
      auto catnames = *meta_.category_names();
      auto catinfo = get_category_info(meta_, fsinfo);
      nlohmann::json& categories = info["categories"];
      for (auto const& [category, ci] : catinfo) {
        std::string name{catnames[category]};
        categories[name] = {
            {"block_count", ci.count},
        };
        if (ci.compressed_size) {
          categories[name]["compressed_size"] = ci.compressed_size.value();
        }
        if (ci.uncompressed_size && !ci.uncompressed_size_is_estimate) {
          categories[name]["uncompressed_size"] = ci.uncompressed_size.value();
        }
      }
    }
  }

  if (opts.features.has(fsinfo_feature::metadata_details)) {
    nlohmann::json meta;

    meta["symlink_inode_offset"] = symlink_inode_offset_;
    meta["file_inode_offset"] = file_inode_offset_;
    meta["dev_inode_offset"] = dev_inode_offset_;
    meta["chunks"] = meta_.chunks().size();
    meta["directories"] = meta_.directories().size();
    meta["inodes"] = meta_.inodes().size();
    meta["chunk_table"] = meta_.chunk_table().size();
    meta["entry_table_v2_2"] = meta_.entry_table_v2_2().size();
    meta["symlink_table"] = meta_.symlink_table().size();
    meta["uids"] = meta_.uids().size();
    meta["gids"] = meta_.gids().size();
    meta["modes"] = meta_.modes().size();
    meta["names"] = meta_.names().size();
    meta["symlinks"] = meta_.symlinks().size();

    if (auto dev = meta_.devices()) {
      meta["devices"] = dev->size();
    }

    if (auto de = meta_.dir_entries()) {
      meta["dir_entries"] = de->size();
    }

    if (auto sfp = meta_.shared_files_table()) {
      if (meta_.options()->packed_shared_files_table()) {
        meta["packed_shared_files_table"] = sfp->size();
        meta["unpacked_shared_files_table"] = shared_files_.size();
      } else {
        meta["shared_files_table"] = sfp->size();
      }
      meta["unique_files"] = unique_files_;
    }

    if (auto history = meta_.metadata_version_history(); history.has_value()) {
      nlohmann::json jhistory = nlohmann::json::array();

      for (auto const& ent : *history) {
        nlohmann::json jent;

        jent["major"] = ent.major();
        jent["minor"] = ent.minor();

        if (ent.dwarfs_version().has_value()) {
          jent["dwarfs_version"] = ent.dwarfs_version().value();
        }

        jent["block_size"] = ent.block_size();

        if (auto entopts = ent.options(); entopts.has_value()) {
          nlohmann::json options;

          parse_fs_options(*entopts, push_back_if_enabled(options));

          jent["options"] = std::move(options);
        }

        time_resolution_handler(ent).add_time_resolution_to(jent);

        jhistory.push_back(std::move(jent));
      }

      meta["metadata_version_history"] = std::move(jhistory);
    }

    info["meta"] = std::move(meta);
  }

  if (opts.features.has(fsinfo_feature::metadata_full_dump)) {
    info["full_metadata"] = nlohmann::json::parse(serialize_as_json(true));
  }

  if (opts.features.has(fsinfo_feature::directory_tree)) {
    info["root"] = as_json(root_);
  }

  return info;
}

// TODO: can we move this to dir_entry_view?
void metadata_v2_data::dump(
    std::ostream& os, std::string const& indent,
    dir_entry_view const& entry, fsinfo_options const& opts,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  auto iv = entry.inode();
  auto inode = iv.inode_num();

  os << indent << "<inode:" << inode << "> " << file_stat::mode_string(iv.mode());

  if (inode > 0) {
    os << " " << entry.name();
  }

  switch (posix_file_type::from_mode(iv.mode())) {
  case posix_file_type::regular: {
    std::error_code ec;
    auto cr = get_chunk_range(inode, ec);
    DWARFS_CHECK(!ec,
                 fmt::format("get_chunk_range({}): {}", inode, ec.message()));
    os << " [" << cr.begin_ << ", " << cr.end_ << "]";
    os << " " << reg_file_size_notrace(iv.raw()) << "\n";
    if (opts.features.has(fsinfo_feature::chunk_details)) {
      icb(indent + "  ", inode);
    }
  } break;

  case posix_file_type::directory:
    dump(os, indent + "  ", make_directory_view(iv), entry, opts, icb);
    break;

  case posix_file_type::symlink:
    os << " -> " << link_value(iv) << "\n";
    break;

  case posix_file_type::block:
    os << " (block device: " << get_device_id(inode).value_or(-1) << ")\n";
    break;

  case posix_file_type::character:
    os << " (char device: " << get_device_id(inode).value_or(-1) << ")\n";
    break;

  case posix_file_type::fifo:
    os << " (named pipe)\n";
    break;

  case posix_file_type::socket:
    os << " (socket)\n";
    break;
  }
}

template <typename LoggerPolicy>
class metadata_ final : public metadata_v2::impl {
 public:
  metadata_(logger& lgr, std::span<uint8_t const> schema,
            std::span<uint8_t const> data, metadata_options const& options,
            int inode_offset, bool force_consistency_check,
            std::shared_ptr<performance_monitor const> const& perfmon)
      : LOG_PROXY_INIT(lgr)
      , data_{LoggerPolicy{},
              lgr,
              schema,
              data,
              options,
              inode_offset,
              force_consistency_check,
              perfmon} {}

  void check_consistency() const override {
    data_.check_consistency(LOG_PROXY_ARG);
  }

  size_t size() const override { return data_.size(); }

  void walk(std::function<void(dir_entry_view)> const& func) const override {
    data_.walk(LOG_PROXY_ARG_ func);
  }

  void walk_data_order(
      std::function<void(dir_entry_view)> const& func) const override {
    data_.walk_data_order(LOG_PROXY_ARG_ func);
  }

  dir_entry_view root() const override { return data_.root(); }

  std::optional<dir_entry_view> find(std::string_view path) const override {
    return data_.find(path);
  }

  std::optional<inode_view> find(int inode) const override {
    return data_.get_entry(inode);
  }

  std::optional<dir_entry_view>
  find(int inode, std::string_view name) const override {
    if (auto iv = data_.get_entry(inode); iv and iv->is_directory()) {
      return data_.find(data_.make_directory_view(*iv), name);
    }

    return std::nullopt;
  }

  file_stat getattr(inode_view iv, std::error_code& /*ec*/) const override {
    return data_.getattr(LOG_PROXY_ARG_ iv);
  }

  file_stat getattr(inode_view iv, getattr_options const& opts,
                    std::error_code& /*ec*/) const override {
    return data_.getattr(LOG_PROXY_ARG_ iv, opts);
  }

  std::optional<directory_view> opendir(inode_view iv) const override {
    std::optional<directory_view> rv;

    if (iv.is_directory()) {
      rv = data_.make_directory_view(iv);
    }

    return rv;
  }

  std::optional<dir_entry_view>
  readdir(directory_view dir, size_t offset) const override {
    return data_.readdir(dir, offset);
  }

  size_t dirsize(directory_view dir) const override {
    return 2 + dir.entry_count();
  }

  void access(inode_view iv, int mode, file_stat::uid_type uid,
              file_stat::gid_type gid, std::error_code& ec) const override {
    LOG_DEBUG << fmt::format("access([{}, {:o}, {}, {}], {:o}, {}, {})",
                             iv.inode_num(), iv.mode(), iv.getuid(),
                             iv.getgid(), mode, uid, gid);

    data_.access(iv, mode, uid, gid, ec);
  }

  int open(inode_view iv, std::error_code& ec) const override {
    if (iv.is_regular_file()) {
      ec.clear();
      return iv.inode_num();
    }

    ec = std::make_error_code(std::errc::invalid_argument);
    return 0;
  }

  file_off_t seek(uint32_t inode, file_off_t offset, seek_whence whence,
                  std::error_code& ec) const override {
    return data_.seek(inode, offset, whence, ec);
  }

  std::string readlink(inode_view iv, readlink_mode mode,
                       std::error_code& ec) const override {
    if (iv.is_symlink()) {
      ec.clear();
      return data_.link_value(iv, mode);
    }

    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }

  void statvfs(vfs_stat* stbuf) const override { data_.statvfs(stbuf); }

  chunk_range get_chunks(int inode, std::error_code& ec) const override {
    return data_.get_chunks(inode, ec);
  }

  size_t block_size() const override { return data_.block_size(); }

  bool has_symlinks() const override { return data_.has_symlinks(); }

  bool has_sparse_files() const override { return data_.has_sparse_files(); }

  nlohmann::json
  get_inode_info(inode_view iv, size_t max_chunks) const override {
    return data_.get_inode_info(iv, max_chunks);
  }

  std::optional<std::string>
  get_block_category(size_t block_number) const override {
    return data_.get_block_category(block_number);
  }

  std::optional<nlohmann::json>
  get_block_category_metadata(size_t block_number) const override {
    return data_.get_block_category_metadata(block_number);
  }

  std::vector<std::string> get_all_block_categories() const override {
    return data_.get_all_block_categories();
  }

  std::vector<file_stat::uid_type> get_all_uids() const override {
    return data_.get_all_uids();
  }

  std::vector<file_stat::gid_type> get_all_gids() const override {
    return data_.get_all_gids();
  }

  std::vector<size_t>
  get_block_numbers_by_category(std::string_view category) const override {
    return data_.get_block_numbers_by_category(category);
  }

  metadata_v2_data const& internal_data() const override { return data_; }

 private:
  LOG_PROXY_DECL(LoggerPolicy);
  metadata_v2_data const data_;
};

chunk_range metadata_v2_data::get_chunks(int inode, std::error_code& ec) const {
  return get_chunk_range(inode - inode_offset_, ec);
}

void metadata_v2_data::dump(
    std::ostream& os, std::string const& indent, directory_view dir,
    dir_entry_view const& entry, fsinfo_options const& opts,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  auto range = dir.entry_range();

  os << " (" << range.size() << " entries, parent=" << dir.parent_entry()
     << ")\n";

  for (auto i : range) {
    dump(os, indent + "  ", make_dir_entry_view(i, entry.raw().self_index()), opts,
         icb);
  }
}

void metadata_v2_data::dump(
    std::ostream& os, fsinfo_options const& opts, filesystem_info const* fsinfo,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  dump(os, "", root_, opts, icb);
}

thrift::metadata::metadata metadata_v2_data::unpack_metadata() const {
  PERFMON_CLS_SCOPED_SECTION(unpack_metadata)

  auto meta = meta_.thaw();

  if (auto opts = meta.options()) {
    if (opts->packed_chunk_table().value()) {
      meta.chunk_table() = chunk_table_.unpack();
    }
    if (auto const& dirs = global_.bundled_directories()) {
      meta.directories() = dirs->thaw();
    }
    if (opts->packed_shared_files_table().value()) {
      meta.shared_files_table() = shared_files_.unpack();
    }
    if (auto const& names = global_.names(); names.is_packed()) {
      meta.names() = names.unpack();
      meta.compact_names().reset();
    }
    if (symlinks_.is_packed()) {
      meta.symlinks() = symlinks_.unpack();
      meta.compact_symlinks().reset();
    }
    opts->packed_chunk_table() = false;
    opts->packed_directories() = false;
    opts->packed_shared_files_table() = false;
  }

  return meta;
}

template <typename LoggerPolicy, typename T>
void metadata_v2_data::walk(LOG_PROXY_REF_(LoggerPolicy) uint32_t self_index,
                            uint32_t parent_index, set_type<int>& seen,
                            T const& func) const {
  func(self_index, parent_index);

  auto entry = make_dir_entry_view_impl(self_index, parent_index);
  auto iv = entry.inode();

  if (iv.is_directory()) {
    auto inode = iv.inode_num();

    if (!seen.emplace(inode).second) {
      DWARFS_THROW(runtime_error, "cycle detected during directory walk");
    }

    auto dir = make_directory_view(inode);

    for (auto cur_index : dir.entry_range()) {
      walk(LOG_PROXY_ARG_ cur_index, self_index, seen, func);
    }

    seen.erase(inode);
  }
}

template <typename LoggerPolicy>
void metadata_v2_data::walk_data_order_impl(LOG_PROXY_REF_(
    LoggerPolicy) std::function<void(dir_entry_view)> const& func) const {
  std::vector<std::pair<uint32_t, uint32_t>> entries;

  {
    auto tv = LOG_TIMED_VERBOSE;

    if (auto dep = meta_.dir_entries()) {
      // 1. collect and partition non-files / files
      entries.resize(dep->size());

      auto const num_files = total_file_entries();
      auto mid = entries.end() - num_files;

      // we use this first to build a mapping from self_index to inode number
      std::vector<uint32_t> first_chunk_block(dep->size());

      {
        auto td = LOG_TIMED_DEBUG;

        size_t other_ix = 0;
        size_t file_ix = entries.size() - num_files;

        walk_tree(LOG_PROXY_ARG_[&, de = *dep, beg = file_inode_offset_,
                                 end = dev_inode_offset_](uint32_t self_index,
                                                         uint32_t parent_index) {
                      int ino = de[self_index].inode_num();
                      size_t index;

                      if (beg <= ino && ino < end) {
                        index = file_ix++;
                        first_chunk_block[self_index] = ino;
                      } else {
                        index = other_ix++;
                      }

                      entries[index] = {self_index, parent_index};
                    });

                    DWARFS_CHECK(
                        file_ix == entries.size(),
                        fmt::format("unexpected file index: {} != {}", file_ix,
                                    entries.size()));
                    DWARFS_CHECK(
                        other_ix == entries.size() - num_files,
                        fmt::format("unexpected other index: {} != {}", other_ix,
                                    entries.size() - num_files));

                    td << "collected " << entries.size() << " entries ("
                       << std::distance(entries.begin(), mid) << " non-files and "
                       << std::distance(mid, entries.end()) << " files)";
                  }

      // 2. order files by chunk block number
      // 2a. build mapping inode -> first chunk block

      {
        auto td = LOG_TIMED_DEBUG;

        for (auto& fcb : first_chunk_block) {
          int ino = fcb;
          if (ino >= file_inode_offset_) {
            ino = file_inode_to_chunk_index(ino);
            if (auto beg = chunk_table_lookup(ino);
                beg != chunk_table_lookup(ino + 1)) {
              fcb = meta_.chunks()[beg].block();
            }
          }
        }

        td << "prepare first chunk block vector";
      }

      // 2b. sort second partition accordingly
      {
        auto td = LOG_TIMED_DEBUG;

        std::stable_sort(mid, entries.end(),
                         [&first_chunk_block](auto const& a, auto const& b) {
                           return first_chunk_block[a.first] <
                                  first_chunk_block[b.first];
                         });

        td << "final sort of " << std::distance(mid, entries.end())
           << " file entries";
      }
    } else {
      entries.reserve(meta_.inodes().size());

      walk_tree(LOG_PROXY_ARG_[&](uint32_t self_index, uint32_t parent_index) {
        entries.emplace_back(self_index, parent_index);
      });

      std::sort(entries.begin(), entries.end(),
                [this](auto const& a, auto const& b) {
                  return meta_.inodes()[a.first].inode_v2_2() <
                         meta_.inodes()[b.first].inode_v2_2();
                });
    }

    tv << "ordered " << entries.size() << " entries by file data order";
  }

  for (auto [self_index, parent_index] : entries) {
    walk_call(func, self_index, parent_index);
  }
}

std::optional<dir_entry_view>
metadata_v2_data::find(directory_view dir, std::string_view name) const {
  PERFMON_CLS_SCOPED_SECTION(find)

  auto range = dir.entry_range();

  if (!options_.case_insensitive_lookup) {
    return find_impl(dir, range, name, std::identity{}, std::identity{});
  }

  auto const& cache = dir_icase_cache_[dir.inode()];

  return find_impl(
      dir, boost::irange(range.size()), utf8_case_fold(name),
      [&cache, &range](auto ix) {
        if (!cache.empty()) {
          ix = cache[ix];
        }
        return range[ix];
      },
      utf8_case_fold_unchecked);
}

std::optional<dir_entry_view>
metadata_v2_data::find(std::string_view path) const {
  PERFMON_CLS_SCOPED_SECTION(find_path)

  auto start = path.find_first_not_of('/');

  if (start != std::string_view::npos) {
    path.remove_prefix(start);
  } else {
    path = {};
  }

  auto dev = std::make_optional(root_);

  while (!path.empty()) {
    auto iv = dev->inode();

    if (!iv.is_directory()) {
      dev.reset();
      break;
    }

    auto name = path;

    if (auto sep = path.find('/'); sep != std::string_view::npos) {
      name = path.substr(0, sep);
      path.remove_prefix(sep + 1);
    } else {
      path = {};
    }

    dev = find(make_directory_view(iv), name);

    if (!dev) {
      break;
    }
  }

  return dev;
}

std::optional<dir_entry_view>
metadata_v2_data::find_impl(directory_view dir, auto const& range,
                            auto const& name, auto const& index_map,
                            auto const& entry_name_transform) const {
  auto entry_name = [&](auto ix) {
    return entry_name_transform(dir_entry_view_impl::name(ix, global_));
  };

  auto it = std::lower_bound(range.begin(), range.end(), name,
                             [&](auto ix, auto const& name) {
                               return entry_name(index_map(ix)) < name;
                             });

  if (it != range.end()) {
    auto ix = index_map(*it);
    if (entry_name(ix) == name) {
      return dir_entry_view{dir_entry_view_impl::from_dir_entry_index_shared(
          ix, global_.self_dir_entry(dir.inode()), global_)};
    }
  }

  return std::nullopt;
}

template <typename LoggerPolicy>
file_stat metadata_v2_data::getattr_impl(LOG_PROXY_REF_(LoggerPolicy)
                                             inode_view const& iv,
                                         getattr_options const& opts) const {
  file_stat stbuf;

  stbuf.set_dev(0); // TODO: should we make this configurable?

  auto mode = iv.mode();
  auto inode = iv.inode_num();

  if (options_.readonly) {
    mode &= READ_ONLY_MASK;
  }

  stbuf.set_mode(mode);

  if (!opts.no_size) {
    auto const sz = stbuf.is_directory()
                        ? file_size_result{static_cast<file_size_t>(
                              make_directory_view(iv).entry_count())}
                        : file_size(LOG_PROXY_ARG_ iv, mode);
    stbuf.set_size(sz.size);
    stbuf.set_blocks((sz.allocated_size + 511) / 512);
    stbuf.set_allocated_size(sz.allocated_size);
  }

  auto& ivr = iv.raw();

  stbuf.set_ino(inode + inode_offset_);
  stbuf.set_blksize(options_.block_size);
  stbuf.set_uid(options_.fs_uid.value_or(iv.getuid()));
  stbuf.set_gid(options_.fs_gid.value_or(iv.getgid()));

  timeres_handler_.fill_stat_timevals(stbuf, ivr);

  if (stbuf.is_regular_file()) {
    if (!nlinks_.has_value()) {
      // nlink values are stored directly in the inode metadata
      stbuf.set_nlink(ivr.nlink_minus_one() + 1);
    } else if (!nlinks_->empty()) {
      // nlink values are stored in a separate table
      stbuf.set_nlink(DWARFS_NOTHROW(nlinks_->at(inode - file_inode_offset_)));
    } else {
      stbuf.set_nlink(1);
    }
  } else {
    stbuf.set_nlink(1);
  }

  stbuf.set_rdev(stbuf.is_device() ? get_device_id(inode).value_or(-1) : 0);

  return stbuf;
}

std::optional<dir_entry_view>
metadata_v2_data::readdir(directory_view dir, size_t offset) const {
  PERFMON_CLS_SCOPED_SECTION(readdir)

  switch (offset) {
  case 0:
    return dir_entry_view{dir_entry_view_impl::from_dir_entry_index_shared(
        global_.self_dir_entry(dir.inode()), global_,
        dir_entry_view_impl::entry_name_type::self)};

  case 1:
    return dir_entry_view{dir_entry_view_impl::from_dir_entry_index_shared(
        global_.self_dir_entry(dir.parent_inode()), global_,
        dir_entry_view_impl::entry_name_type::parent)};

  default:
    offset -= 2;

    if (offset >= dir.entry_count()) {
      break;
    }

    auto index = dir.first_entry() + offset;
    return dir_entry_view{dir_entry_view_impl::from_dir_entry_index_shared(
        index, global_.self_dir_entry(dir.inode()), global_)};
  }

  return std::nullopt;
}

void metadata_v2_data::access(inode_view const& iv, int mode,
                              file_stat::uid_type uid, file_stat::gid_type gid,
                              std::error_code& ec) const {
  if (mode == F_OK) {
    // easy; we're only interested in the file's existence
    ec.clear();
    return;
  }

  int access_mode = 0;

  auto set_xok = [&access_mode]() {
#ifdef _WIN32
    access_mode |= 1; // Windows has no notion of X_OK
#else
    access_mode |= X_OK;
#endif
  };

  if (uid == 0) {
    access_mode = R_OK | W_OK;

    if (iv.mode() & uint16_t(fs::perms::owner_exec | fs::perms::group_exec |
                             fs::perms::others_exec)) {
      set_xok();
    }
  } else {
    auto test = [e_mode = iv.mode(), &access_mode, &set_xok,
                 readonly = options_.readonly](fs::perms r_bit, fs::perms w_bit,
                                               fs::perms x_bit) {
      if (e_mode & uint16_t(r_bit)) {
        access_mode |= R_OK;
      }
      if (e_mode & uint16_t(w_bit)) {
        if (!readonly) {
          access_mode |= W_OK;
        }
      }
      if (e_mode & uint16_t(x_bit)) {
        set_xok();
      }
    };

    // Let's build the inode's access mask
    test(fs::perms::others_read, fs::perms::others_write,
         fs::perms::others_exec);

    if (iv.getgid() == gid) {
      test(fs::perms::group_read, fs::perms::group_write,
           fs::perms::group_exec);
    }

    if (iv.getuid() == uid) {
      test(fs::perms::owner_read, fs::perms::owner_write,
           fs::perms::owner_exec);
    }
  }

  if ((access_mode & mode) == mode) {
    ec.clear();
  } else {
    ec = std::make_error_code(std::errc::permission_denied);
  }
}

nlohmann::json metadata_v2_data::get_inode_info(inode_view const& iv,
                                                size_t max_chunks) const {
  nlohmann::json obj;

  if (iv.is_regular_file()) {
    std::error_code ec;
    auto chunk_range = get_chunk_range(iv.inode_num(), ec);

    DWARFS_CHECK(!ec, fmt::format("get_chunk_range({}): {}", iv.inode_num(),
                                  ec.message()));

    if (chunk_range.size() <= max_chunks) {
      for (auto const& chunk : chunk_range) {
        nlohmann::json& chk = obj["chunks"].emplace_back();

        if (chunk.is_data()) {
          chk["kind"] = "data";
          chk["block"] = chunk.block();
          chk["offset"] = chunk.offset();
          chk["size"] = chunk.size();

          if (auto catname = get_block_category(chunk.block())) {
            chk["category"] = catname.value();
          }
        } else {
          chk["kind"] = "hole";
          chk["size"] = chunk.size();
        }
      }
    } else {
      obj["chunks"] = fmt::format("too many chunks ({})", chunk_range.size());
    }
  }

  obj["mode"] = iv.mode();
  obj["modestring"] = file_stat::mode_string(iv.mode());
  obj["uid"] = iv.getuid();
  obj["gid"] = iv.getgid();

  return obj;
}

metadata_v2_utils::metadata_v2_utils(metadata_v2 const& meta)
    : data_{meta.internal_data()} {}

void metadata_v2_utils::dump(
    std::ostream& os, fsinfo_options const& opts, filesystem_info const* fsinfo,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  data_.dump(os, opts, fsinfo, icb);
}

nlohmann::json
metadata_v2_utils::info_as_json(fsinfo_options const& opts,
                                filesystem_info const* fsinfo) const {
  return data_.info_as_json(opts, fsinfo);
}

nlohmann::json metadata_v2_utils::as_json() const { return data_.as_json(); }

std::string metadata_v2_utils::serialize_as_json(bool simple) const {
  return data_.serialize_as_json(simple);
}

#ifdef DWARFS_HAVE_THRIFT
std::unique_ptr<thrift::metadata::metadata> metadata_v2_utils::thaw() const {
  return data_.thaw();
}

std::unique_ptr<thrift::metadata::metadata> metadata_v2_utils::unpack() const {
  return data_.unpack();
}

std::unique_ptr<thrift::metadata::fs_options>
metadata_v2_utils::thaw_fs_options() const {
  return data_.thaw_fs_options();
}

metadata_v2::metadata_v2(
    logger& lgr, std::span<uint8_t const> schema, std::span<uint8_t const> data,
    metadata_options const& options, int inode_offset,
    bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon)
    : impl_(make_unique_logging_object<metadata_v2::impl, metadata_,
                                       logger_policies>(
          lgr, schema, data, options, inode_offset, force_consistency_check,
          perfmon)) {}

chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
#endif

} // namespace dwarfs::reader::internal
