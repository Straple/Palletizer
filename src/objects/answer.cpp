#include <objects/answer.hpp>

std::ostream &operator<<(std::ostream &output, const Answer &answer) {
    output << "SKU,x,y,z,X,Y,Z\n";
    for (auto &pos: answer.positions) {
        output << pos.sku << ',' << pos.x << ',' << pos.y << ',' << pos.z << ',' << pos.X << ',' << pos.Y << ','
               << pos.Z << '\n';
    }
    return output;
}
