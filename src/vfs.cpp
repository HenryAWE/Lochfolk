#include <lochfolk/vfs.hpp>
#include <memory>
#include <cassert>
#include <fstream>
#include <sstream>
#include <lochfolk/utility.hpp>

namespace lochfolk
{
namespace detail
{
    std::string stdfs_err_msg(
        std::string_view prefix,
        const std::filesystem::path& p,
        std::string_view suffix = {}
    )
    {
#ifdef _WIN32
        auto tmp = p.u8string();
        std::string_view p_sv(
            reinterpret_cast<const char*>(tmp.c_str()), tmp.size()
        );

#else
        std::string_view p_sv = p.native();

#endif

        std::string result;
        result.reserve(
            prefix.size() + p_sv.size() + suffix.size() + 2 /* "" */
        );

        result += prefix;
        result += '"';
        result += p_sv;
        result += '"';
        result += suffix;

        return result;
    }

    std::string stdfs_err_msg(
        const std::filesystem::path& p,
        std::string_view suffix = {}
    )
    {
        return stdfs_err_msg(
            std::string_view(), p, suffix
        );
    }

    std::string vfs_err_msg(
        std::string_view prefix,
        path_view p,
        std::string_view suffix = {}
    )
    {
        std::string_view p_sv = p;

        std::string result;
        result.reserve(
            prefix.size() + p_sv.size() + suffix.size() + 2 /* "" */
        );

        result += prefix;
        result += '"';
        result += p_sv;
        result += '"';
        result += suffix;

        return result;
    }

    std::string vfs_err_msg(
        path_view p,
        std::string_view suffix = {}
    )
    {
        return vfs_err_msg(
            std::string_view(), p, suffix
        );
    }
} // namespace detail

std::uint64_t virtual_file_system::file_node::directory::file_size() const noexcept
{
    return 0;
}

std::unique_ptr<std::streambuf> virtual_file_system::file_node::string_constant::open(
    std::ios_base::openmode mode
) const
{
    mode &= ~std::ios_base::out;

    return std::visit(
        [mode]<typename T>(const T& v) -> std::unique_ptr<std::streambuf>
        {
            if constexpr(std::same_as<std::remove_cvref_t<T>, std::string_view>)
            {
                std::span<char> sp(const_cast<char*>(v.data()), v.size());
                return std::make_unique<span_buf>(sp, mode);
            }
            else // std::string
            {
                return std::make_unique<std::stringbuf>(v, mode);
            }
        },
        m_str_data
    );
}

std::string virtual_file_system::file_node::string_constant::read_string(bool convert_crlf) const
{
    (void)convert_crlf;
    return std::string(view());
}

std::uint64_t virtual_file_system::file_node::string_constant::file_size() const
{
    return std::visit(
        [](const auto& v) -> std::uint64_t
        { return static_cast<std::uint64_t>(v.size()); },
        m_str_data
    );
}

std::string_view virtual_file_system::file_node::string_constant::view() const noexcept
{
    return std::visit(
        [](const auto& v) -> std::string_view
        { return v; },
        m_str_data
    );
}

std::unique_ptr<std::filebuf> virtual_file_system::file_node::sys_file::open(
    std::ios_base::openmode mode
) const
{
    std::unique_ptr fb = std::make_unique<std::filebuf>();
    fb->open(m_sys_path, mode);
    if(!fb->is_open())
        throw error(detail::stdfs_err_msg("failed to open ", m_sys_path));

    return fb;
}

std::string virtual_file_system::file_node::sys_file::read_string(bool convert_crlf) const
{
    std::ios_base::openmode mode = std::ios_base::in;
    if(!convert_crlf)
        mode |= std::ios_base::binary;

    std::ifstream ifs(m_sys_path, mode);
    if(!ifs.is_open()) [[unlikely]]
        throw error(detail::stdfs_err_msg("failed to open ", m_sys_path));

    std::stringstream ss;
    ss << ifs.rdbuf();
    return std::move(ss).str();
}

std::uint64_t virtual_file_system::file_node::sys_file::file_size() const
{
    return static_cast<std::uint64_t>(
        std::filesystem::file_size(m_sys_path)
    );
}

virtual_file_system::file_node::archive_entry::archive_entry(archive& ar, std::int64_t off)
    : m_archive_ref(ar.shared_from_this()), m_offset(off)
{}

virtual_file_system::file_node::archive_entry& virtual_file_system::file_node::archive_entry::operator=(
    archive_entry&& rhs
) noexcept
{
    if(this == &rhs) [[unlikely]]
        return *this;

    m_archive_ref = std::move(rhs.m_archive_ref);
    m_offset = std::exchange(rhs.m_offset, 0);

    return *this;
}

std::unique_ptr<std::streambuf> virtual_file_system::file_node::archive_entry::open(
    std::ios_base::openmode mode
) const
{
    return m_archive_ref->getbuf(m_offset, mode);
}

std::string virtual_file_system::file_node::archive_entry::read_string(bool convert_crlf) const
{
    (void)convert_crlf;
    return m_archive_ref->read_string(m_offset);
}

std::uint64_t virtual_file_system::file_node::archive_entry::file_size() const
{
    return m_archive_ref->get_file_size(m_offset);
}

bool virtual_file_system::file_node::is_directory() const noexcept
{
    return std::holds_alternative<directory>(m_data);
}

std::uint64_t virtual_file_system::file_node::file_size() const
{
    return std::visit(
        [](const auto& v) -> std::uint64_t
        {
            return v.file_size();
        },
        m_data
    );
}

std::unique_ptr<std::streambuf> virtual_file_system::file_node::getbuf(
    std::ios_base::openmode mode
) const
{
    return visit(
        [mode]<typename T>(const T& v) -> std::unique_ptr<std::streambuf>
        {
            constexpr bool has_buf = requires() { v.open(mode); };
            if constexpr(has_buf)
                return v.open(mode);
            throw error("bad file");
        }
    );
}

std::string virtual_file_system::file_node::read_string(bool convert_crlf) const
{
    return visit(
        [convert_crlf]<typename T>(const T& v) -> std::string
        {
            constexpr bool has_read_string = requires() { v.read_string(convert_crlf); };
            if constexpr(has_read_string)
                return v.read_string(convert_crlf);
            throw error("bad file");
        }
    );
}

virtual_file_system::virtual_file_system()
    : m_root(nullptr, std::in_place_type<file_node::directory>)
{
    assert(m_root.is_directory());
}

virtual_file_system::~virtual_file_system() = default;

void virtual_file_system::mount_string_constant(
    path_view p, std::string_view str, bool overwrite
)
{
    mount_impl(
        p,
        overwrite,
        std::in_place_type<file_node::string_constant>,
        str
    );
}

void virtual_file_system::mount_string_constant(
    path_view p, const char* str, bool overwrite
)
{
    mount_string_constant(
        p,
        std::string_view(str),
        overwrite
    );
}

void virtual_file_system::mount_string_constant(
    path_view p, std::string str, bool overwrite
)
{
    mount_impl(
        p,
        overwrite,
        std::in_place_type<file_node::string_constant>,
        std::move(str)
    );
}

void virtual_file_system::mount_sys_file(
    path_view p, const std::filesystem::path& sys_path, bool overwrite
)
{
    namespace stdfs = std::filesystem;

    if(!stdfs::exists(sys_path))
    {
        throw error(detail::stdfs_err_msg(sys_path, " does not exist"));
    }
    else if(stdfs::is_regular_file(sys_path))
    {
        mount_impl(
            p,
            overwrite,
            std::in_place_type<file_node::sys_file>,
            stdfs::absolute(sys_path)
        );
    }
    else
    {
        throw error(detail::stdfs_err_msg({}, sys_path, " is not a regular file"));
    }
}

void virtual_file_system::mount_sys_dir(
    path_view p, const std::filesystem::path& dir, bool overwrite
)
{
    namespace stdfs = std::filesystem;

    if(!stdfs::is_directory(dir))
    {
        throw error(detail::stdfs_err_msg(dir, " is not a directory"));
    }

    path base(p);
    for(auto& i : stdfs::recursive_directory_iterator(dir))
    {
        if(stdfs::is_directory(i))
            continue;

        stdfs::path file_path = stdfs::absolute(i);
        std::u8string filename = stdfs::relative(file_path, dir).generic_u8string();
        mount_impl(
            base / std::string_view((const char*)filename.c_str(), filename.size()),
            overwrite,
            std::in_place_type<file_node::sys_file>,
            file_path
        );
    }
}

void virtual_file_system::mount_zip_archive(
    path_view p, const std::filesystem::path& sys_path, bool overwrite
)
{
    std::shared_ptr ar = std::make_shared<zip_archive>();
    ar->open(sys_path);

    path base(p);

    if(!ar->goto_first())
        return; // Empty archive
    do
    {
        if(ar->current_is_dir())
            continue;

        auto entry = ar->open_current();
        std::string_view filename = entry.filename();

        mount_impl(
            base / filename,
            overwrite,
            std::in_place_type<file_node::archive_entry>,
            *ar,
            entry.offset()
        );
    } while(ar->goto_next());
}

bool virtual_file_system::exists(path_view p) const
{
    return find_impl(p) != nullptr;
}

bool virtual_file_system::is_directory(path_view p) const
{
    const file_node* f = find_impl(p);
    if(!f)
        return false;
    return f->is_directory();
}

std::uint64_t virtual_file_system::file_size(path_view p) const
{
    const file_node* f = find_impl(p);
    if(!f) [[unlikely]]
        throw error(detail::vfs_err_msg(p, " is not found"));

    return f->file_size();
}

bool virtual_file_system::remove(path_view p)
{
    if(p.empty() || !p.is_absolute()) [[unlikely]]
        return false;
    if(p == "/"_pv)
    {
        auto* root_dir = m_root.get_if<file_node::directory>();
        assert(root_dir);

        root_dir->children().clear();
        return true;
    }

    auto* parent = find_impl(p.parent_path());
    if(!parent->is_directory())
        return false;

    std::string_view target_sv = [](path_view pv)
    {
        std::string_view result(pv);
        if(result.back() == path_view::separator)
            result.remove_suffix(1);

        std::size_t pos = result.rfind(path_view::separator);
        if(pos == std::string_view::npos)
            pos = 0;
        return result.substr(pos + 1);
    }(p);

    auto* parent_dir = parent->get_if<file_node::directory>();
    auto it = parent_dir->children().find(path_view(target_sv));
    if(it == parent_dir->children().end())
        return false;

    parent_dir->children().erase(it);
    return true;
}

ivfstream virtual_file_system::open(path_view p, std::ios_base::openmode mode)
{
    const file_node* f = find_impl(p);
    if(!f)
        throw error(detail::vfs_err_msg(p, " is not found"));

    mode |= std::ios_base::in;
    return ivfstream(f->getbuf(mode));
}

std::string virtual_file_system::read_string(path_view p, bool convert_crlf)
{
    const file_node* f = find_impl(p);
    if(!f)
        throw error(detail::vfs_err_msg(p, " is not found"));

    return f->read_string(convert_crlf);
}

void virtual_file_system::list_files(std::ostream& os)
{
    list_files_impl(os, "/", m_root, 0);
}

void virtual_file_system::list_files_impl(std::ostream& os, std::string_view name, const file_node& f, unsigned int indent)
{
    for(unsigned int i = 0; i < indent; ++i)
        os << "  ";
    bool is_dir = f.is_directory();
    os << "- " << name;
    if(is_dir && name != "/")
        os << '/';
    os << '\n';

    if(is_dir)
    {
        auto* dir = f.get_if<file_node::directory>();
        assert(dir != nullptr);
        for(const auto& [sub_name, sub_f] : dir->children())
        {
            list_files_impl(os, sub_name, sub_f, indent + 1);
        }
    }
}

const virtual_file_system::file_node* virtual_file_system::find_impl(path_view p) const
{
    if(p.empty() || !p.is_absolute()) [[unlikely]]
        return nullptr;
    if(p == "/"_pv)
        return &m_root;

    const file_node* current = &m_root;
    for(path_view subview : p.split_view())
    {
        auto* dir = current->get_if<file_node::directory>();
        if(!dir)
            return nullptr;

        auto it = dir->children().find(std::string_view(subview));
        if(it == dir->children().end())
            return nullptr;

        current = &it->second;
    }

    return current;
}

const virtual_file_system::file_node* virtual_file_system::mkdir_impl(path_view p)
{
    const file_node* current = &m_root;
    for(path_view subview : p.split_view())
    {
        assert(current->is_directory());
        auto* dir = current->get_if<file_node::directory>();
        assert(dir);

        auto it = dir->children().find(std::string_view(subview));
        if(it != dir->children().end())
        {
            if(!it->second.is_directory())
            {
                throw error(detail::vfs_err_msg(subview, " already exists"));
            }
        }
        else
        {
            it = dir->children().emplace_hint(
                it,
                std::string(subview),
                file_node(current, std::in_place_type<file_node::directory>)
            );
        }

        current = &it->second;
    }

    return current;
}

template <typename T, typename... Args>
auto virtual_file_system::mount_impl(
    path_view p, bool overwrite, std::in_place_type_t<T>, Args&&... args
) -> std::pair<const file_node*, bool>
{
    static_assert(!std::same_as<T, file_node::directory>, "Cannot mount a directory");
    assert(p.is_absolute());
    const file_node* current = mkdir_impl(p.parent_path());
    path_view filename = p.filename();

    auto* dir = current->get_if<file_node::directory>();
    assert(dir);
    auto it = dir->children().find(filename);
    if(it != dir->children().end())
    {
        if(overwrite)
        {
            it->second = file_node(
                current, std::in_place_type<T>, std::forward<Args>(args)...
            );

            return std::make_pair(&it->second, true);
        }

        return std::make_pair(&it->second, false);
    }
    else
    {
        auto result = dir->children().emplace(
            filename,
            file_node(current, std::in_place_type<T>, std::forward<Args>(args)...)
        );
        assert(result.second); // Emplacement should be successful here

        return std::make_pair(
            &result.first->second,
            true
        );
    }
}

access_context::access_context(const access_context&) = default;

access_context::access_context(virtual_file_system& vfs)
    : m_vfs(&vfs), m_current("/") {}

void access_context::current_path(path_view pv)
{
    m_current /= pv;
    m_current = m_current.lexically_normal();
}

path access_context::to_fullpath(path_view pv) const
{
    path result = m_current / pv;

    return result.lexically_normal();
}
} // namespace lochfolk
