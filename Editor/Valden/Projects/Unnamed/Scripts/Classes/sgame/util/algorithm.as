// algorithm.as -- various utility functons like the STL but for angelscript

uint find( vector<int>@ v, int value ) {
    uint index = 0;
    for ( auto it = v.begin(); it != v.end(); it++, index++ ) {
        if ( it == value ) {
            break;
        }
    }
    return index;
}

uint find( vector<TheNomad::SGame::EntityObject@>@ v, const TheNomad::SGame::EntityObject@ value ) {
    uint index = 0;
    for ( auto it = v.begin(); it != v.end(); it++, index++ ) {
        if ( it == value ) {
            break;
        }
    }
    return index;
}
