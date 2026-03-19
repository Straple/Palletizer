#include <objects/answer.hpp>

std::ostream &operator<<(std::ostream &output, const Answer &answer) {
    output << "Pallet,SKU,x,y,z,X,Y,Z\n";
    for (uint32_t p = 0; p < answer.pallets.size(); p++) {
        for (auto &pos: answer.pallets[p]) {
            output << p << ',' << pos.sku << ',' << pos.x << ',' << pos.y << ',' << pos.z << ',' << pos.X << ','
                   << pos.Y << ',' << pos.Z << '\n';
        }
    }
    return output;
}
