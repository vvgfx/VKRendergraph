#pragma once

#include "RendergraphBuilder.h"

namespace rgraph
{
    /**
     * @brief This class represents a feature for the rendergraph
     *
     */
    class IFeature
    {
      public:
        /**
         * @brief Register the feature with the rendergraph.
         *
         */
        virtual void Register(Rendergraph *builder) = 0;
    };
} // namespace rgraph