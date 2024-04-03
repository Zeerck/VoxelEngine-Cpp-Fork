#include "UiDocument.h"

#include "../files/files.h"
#include "../graphics/ui/elements/containers.h"
#include "../graphics/ui/elements/UINode.h"
#include "../graphics/ui/gui_xml.h"
#include "../logic/scripting/scripting.h"
#include "InventoryView.h"

UiDocument::UiDocument(
    std::string id, 
    uidocscript script, 
    std::shared_ptr<gui::UINode> root,
    std::unique_ptr<scripting::Environment> env
) : id(id), script(script), root(root), env(std::move(env)) {
    buildIndices(map, root);
}

void UiDocument::rebuildIndices() {
    buildIndices(map, root);
}

const uinodes_map& UiDocument::getMap() const {
    return map;
}

const std::string& UiDocument::getId() const {
    return id;
}

const std::shared_ptr<gui::UINode> UiDocument::getRoot() const {
    return root;
}

const std::shared_ptr<gui::UINode> UiDocument::get(const std::string& id) const {
    auto found = map.find(id);
    if (found == map.end()) {
        return nullptr;
    }
    return found->second;
}

const uidocscript& UiDocument::getScript() const {
    return script;
}

int UiDocument::getEnvironment() const {
    return env->getId();
}

void UiDocument::buildIndices(uinodes_map& map, std::shared_ptr<gui::UINode> node) {
    const std::string& id = node->getId();
    if (!id.empty()) {
        map[id] = node;
    }
    auto container = std::dynamic_pointer_cast<gui::Container>(node);
    if (container) {
        for (auto subnode : container->getNodes()) {
            buildIndices(map, subnode);
        }
    }
}

std::unique_ptr<UiDocument> UiDocument::read(int penv, std::string name, fs::path file) {
    const std::string text = files::read_string(file);
    auto xmldoc = xml::parse(file.u8string(), text);

    auto env = penv == -1 
        ? std::make_unique<scripting::Environment>(0) 
        : scripting::create_doc_environment(penv, name);

    gui::UiXmlReader reader(*env);
    InventoryView::createReaders(reader);
    auto view = reader.readXML(
        file.u8string(), xmldoc->getRoot()
    );
    view->setId("root");
    uidocscript script {};
    auto scriptFile = fs::path(file.u8string()+".lua");
    if (fs::is_regular_file(scriptFile)) {
        scripting::load_layout_script(env->getId(), name, scriptFile, script);
    }
    return std::make_unique<UiDocument>(name, script, view, std::move(env));
}

std::shared_ptr<gui::UINode> UiDocument::readElement(fs::path file) {
    auto document = read(-1, file.filename().u8string(), file);
    return document->getRoot();
}
