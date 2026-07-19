#include "dwarfs_loader.h"
#include <iostream>

int main() {
    using namespace static_site_server;
    
    dwarfs_loader_config config;
    config.image_path = "aesop.dff";
    config.cache_size = 128 << 20;
    config.num_workers = 4;
    
    auto loader = dwarfs_loader::create(config);
    if (!loader) {
        std::cerr << "Failed to create loader\n";
        return 1;
    }
    
    std::cout << "Loader created: " << loader->get_info() << "\n";
    
    auto content = loader->get_file("/pg11339-images.html");
    if (content) {
        std::cout << "File found: " << content->size << " bytes\n";
        std::cout << "MIME: " << content->mime_type << "\n";
        std::cout << "First 100 chars: " << content->data.substr(0, 100) << "\n";
    } else {
        std::cerr << "File not found\n";
        return 1;
    }
    
    return 0;
}
