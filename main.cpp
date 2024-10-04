///
/// @author Jonathan Pence
/// @date 2024-10-03
/// @brief CLI tool for fetching and displaying Simplestream information.
///

#include <ranges>
#include <jsoncpp/json/json.h>
#include <httplib.h>

// These values can easily be changed to modify the behaviour of this tool.
constexpr const char *SIMPLESTREAM_HOST = "cloud-images.ubuntu.com";
constexpr const char *SIMPLESTREAM_PATH = "/releases/streams/v1/com.ubuntu.cloud:released:download.json";
constexpr const char *ARCH_NAME         = "amd64";
constexpr const char *IMAGE_TAG         = "disk1.img";
constexpr const char *INFO_TAG          = "sha256";

///
/// @brief A collection of static helper methods for type-checked access to 
/// JSON data. 
/// @details Throws a std::runtime_error with some helpful info when an
/// access fails.
///
class JsonAccessors {
public:
    static const Json::Value& getObject(const Json::Value &val, const std::string &key) {
        const Json::Value &ret = val[key];
        if (!ret.isObject())
            throw std::runtime_error(key + " is not an object");
        return ret;
    }

    static Json::String getString(const Json::Value &val, const std::string &key) {
        const Json::Value &ret = val[key];
        if (!ret.isString())
            throw std::runtime_error(key + " is not a string");
        return ret.asString();
    }

    static bool getBool(const Json::Value &val, const std::string &key) {
        const Json::Value &ret = val[key];
        if (!ret.isBool())
            throw std::runtime_error(key + " is not a boolean");
        return ret.asBool();
    }

    static Json::String getLastMemberName(const Json::Value &val) {
        Json::Value::Members members = val.getMemberNames();
        if (members.empty())
            throw std::runtime_error("object has no members");
        return members.back();
    }
};

///
/// @brief Provides easy access to relevant product details.
/// @details Stores a reference to a Json::Value to a member of the "products"
/// object in the Simplestream JSON document.
///
class Product : private JsonAccessors {
public:
    explicit Product(const Json::Value &val) : m_prod(val) {}
    Product(const Product &other) : m_prod(other.m_prod) {}

    explicit operator bool() const { return !!m_prod; }

    bool getSupported() const { return getBool(m_prod, "supported"); }
    Json::String getAliases() const { return getString(m_prod, "aliases"); }
    Json::String getRelease() const { return getString(m_prod, "release"); }
    Json::String getReleaseTitle() const { return getString(m_prod, "release_title"); }
    Json::String getVersion() const { return getString(m_prod, "version"); }
    
    Json::String getPubname(const std::string &rev = {}) const {
        const auto &revision = getRevisionObject(rev);
        return getString(revision, "pubname");
    }

    Json::String getImageInfo(const std::string &rev = {}) const {
        const auto &revision = getRevisionObject(rev);
        const auto &items = getObject(revision, "items");
        const auto &image = getObject(items, IMAGE_TAG);
        return getString(image, INFO_TAG);
    }

private:
    Product(Product&&) = delete;

    const Json::Value& getRevisionObject(std::string revision) const {
        const auto &versions = getObject(m_prod, "versions");
        if (revision.empty()) {
            revision = getLastMemberName(versions);
        }
        return getObject(versions, revision);
    }
    
    const Json::Value &m_prod;
};

///
/// @brief Provides high-level access to relevant products in a Simplestream
///  JSON document.
///
class Simplestream : private JsonAccessors {
public:
    using Products = std::vector<Product>;

    explicit Simplestream(const std::string &document) {
        Json::Reader reader;
        if (!reader.parse(document, m_root))
            throw std::runtime_error(reader.getFormattedErrorMessages());
    }
    
    Products getProducts() const {
        const auto &products = getObject(m_root, "products");
        // Only concerned with amd64 architecture for cloud images
        auto filter = [](const Json::String &prod) { return prod.ends_with(ARCH_NAME); };
        std::ranges::filter_view productNames{products.getMemberNames(), filter};
        
        Products ret;
        for (const auto &prodName : productNames) {
            ret.emplace_back(getObject(products, prodName));
        }
        return ret;
    }

    Products getSupportedProducts() const {
        Products ret;
        const Products prods = getProducts();
        for (const auto &prod : prods) {
            if (prod.getSupported()) {
                ret.emplace_back(prod);
            }
        }
        return ret;
    }

    Product getCurrentProduct() const {
        const Products prods = getProducts();
        for (const auto &prod : prods) {
            auto aliases = prod.getAliases();
            // I think "current" best equates to the "default" product. Could
            //  the latest product could be a pre-release?
            if (aliases.find("default") != aliases.npos) {
                return prod;
            }
        }
        return Product(Json::nullValue);
    }

    Product findProduct(const std::string_view &release) {
        const Products prods = getProducts();
        for (const auto &prod : prods) {
            // Split comma-separated "aliases" into a range-view and filter
            //  out "lts" since that's common to multiple products.
            const auto alias = prod.getAliases();
            auto aliases = alias |
                std::views::lazy_split(',') |
                std::views::filter([](auto a) {
                    return !std::ranges::equal(a, std::string_view("lts"));
                });
            // `release` matches any of a product's aliases (e.g. "noble", "default")
            for (auto a : aliases) {
                if (std::ranges::equal(a, release)) {
                    return prod;
                }
            }
            // `release` contains a version string (e.g. "Ubuntu-24.04")
            if (release.find(prod.getVersion()) != release.npos) {
                return prod;
            }
        }
        return Product(Json::nullValue);
    }

private:
    // Prevent default copy and move constructors.
    Simplestream(const Simplestream&) = delete;
    Simplestream(Simplestream&&) = delete;
    
    Json::Value m_root;
};

///
/// @brief Display help text
///
void printUsage()
{
    std::cout << "Usage: simplestream [OPTION]... <release>...\n";
    std::cout << "Prints the latest Ubuntu Cloud image information.\n\n";
    std::cout << "  -l, --list                  List currently supported Ubuntu releases\n";
    std::cout << "  -c, --current               Current Ubuntu LTS version\n";
    std::cout << "  -s, --sha256 <release>...   SHA256 checksum of disk1.img\n";
    std::cout << "  -h, --help                  Display this help and exit\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  release                     Release version, name, or initial\n\n";
}

///
/// @brief CLI for fetching and displaying Simplestream information
/// @param argc 
/// @param argv 
/// @return exit status
///
int main(int argc, char *argv[])
{
    // Vectorize command line arguments
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    if (args.empty()) {
        printUsage();
        return EXIT_FAILURE;
    }

    // Option flags
    bool list = false;
    bool current = false;
    bool sha256 = false;
    bool usage = false;
    // Release argument(s) for sha256 option
    std::vector<std::string_view> releases;
    // Parse command line arguments. Short options can be stacked (e.g. -lc).
    for (auto arg : args) {
        bool parsed = false;
        bool dashed = arg.starts_with('-');
        // After receiving the sha256 option, any argument not starting with a
        //  dash is treated as a <release> argument.
        if (sha256 && !dashed) {
            parsed = true;
            releases.push_back(arg);
        }
        // Don't look for short options inside long options.
        dashed = dashed && !arg.starts_with("--");
        if (arg == "--list" || (dashed && arg.find('l') != arg.npos)) {
            parsed = list = true;
        }
        if (arg == "--current" || (dashed && arg.find('c') != arg.npos)) {
            parsed = current = true;
        }
        if (arg == "--sha256" || (dashed && arg.find('s') != arg.npos)) {
            parsed = sha256 = true;
        }
        if (arg == "--help" || (dashed && arg.find('h') != arg.npos)) {
            parsed = usage = true;
        }
        if (!parsed) {
            std::cout << "unrecognized argument: " << arg << std::endl;
            printUsage();
            return EXIT_FAILURE;
        }
    }

    // -h, --help
    // Print and exit before downloading and parsing JSON
    if (usage) {
        printUsage();
        return EXIT_SUCCESS;
    }

    // Fetch the latest Ubuntu Cloud image information
    httplib::SSLClient client(SIMPLESTREAM_HOST);
    auto reply = client.Get(SIMPLESTREAM_PATH);
    if (!reply) {
        std::cout << "error code: " << reply.error() << std::endl;
        auto result = client.get_openssl_verify_result();
        if (result) {
            std::cout << "verify error: " << X509_verify_cert_error_string(result) << std::endl;
        }
        return EXIT_FAILURE;
    }

    try {
        // Parse Simplestream formatted JSON from the reply
        Simplestream stream(reply->body);
        
        // -l, --list
        if (list) {
            std::cout << "Suported Ubuntu releases:" << std::endl;
            const auto releases = stream.getSupportedProducts();
            for (const auto &rel : releases) {
                std::cout << "  " << rel.getReleaseTitle(); 
                std::cout << " (" << rel.getRelease() << ")\n"; 
            }
        }

        // -c, -current
        if (current) {
            const auto &prod = stream.getCurrentProduct();
            std::cout << "Current Ubuntu LTS version: " << prod.getVersion() << std::endl;
            std::cout << "  " << prod.getPubname() << std::endl;
        }

        // -s, --sha256 <release>...
        if (sha256) {
            if (releases.empty()) {
                std::cout << "error: No release specified.\n\n";
                printUsage();
                return EXIT_FAILURE;
            }
            for (const auto &release : releases) {
                const auto &prod = stream.findProduct(release);
                if (prod) {
                    std::cout << "SHA256 checksum for " << IMAGE_TAG << " of " << prod.getPubname() << ":\n";
                    std::cout << "  " << prod.getImageInfo() << std::endl;
                } else {
                    std::cout << "error: Release \"" << release << "\" not found.\n";
                }
            }
        }
    } catch (const std::runtime_error& err) {
        std::cout << "error: " << err.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
