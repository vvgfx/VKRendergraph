#include "Scenegraph.h"
#include "ScenegraphStructs.h"
#include <memory>
#include <optional>
#include <string>

using namespace sgraph;

void Scenegraph::makeScenegraph(std::unordered_map<std::string, std::shared_ptr<INode>> scenegraphNodes)
{
    this->nodes = scenegraphNodes;
}

std::shared_ptr<INode> Scenegraph::getRoot()
{
    return root;
}

std::optional<std::shared_ptr<INode>> Scenegraph::getNode(std::string name)
{
    if (nodes.contains(name))
    {
        return nodes[name];
    }

    return std::nullopt;
}

void Scenegraph::setRoot(std::shared_ptr<INode> root)
{
    this->root = root;
}

Scenegraph::~Scenegraph()
{
    nodes.clear();
}

void Scenegraph::cleanup()
{
    nodes.clear();
}