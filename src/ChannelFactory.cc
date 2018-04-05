#include "ChannelFactory.h"

namespace Chili {

std::shared_ptr<ChannelFactory> ChannelFactory::Create(ChannelProcessCallback process) {
    class CustomChannel : public Channel {
    public:
        CustomChannel(std::shared_ptr<FileStream> fs, ChannelProcessCallback process)
            : Channel(std::move(fs))
            , _process(std::move(process)) {}

        void Process() override {
            _process(*this);
        }

    private:
        ChannelProcessCallback _process;
    };

    class CustomFactory : public ChannelFactory {
    public:
        CustomFactory(ChannelProcessCallback process)
            : _process(std::move(process)) {}

        std::shared_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
            return std::make_shared<CustomChannel>(std::move(fs), _process);
        }

    private:
        ChannelProcessCallback _process;
    };

    return std::make_shared<CustomFactory>(std::move(process));
}

} // namespace Chili

