
// program to generate limits and data for every piece for c in the form of arrays
const tris_pieces_places = [
  [ [ 0, 1, 1, 1, 2, 1, 3, 1 ], [ 2, 0, 2, 1, 2, 2, 2, 3 ], [ 0, 2, 1, 2, 2, 2, 3, 2 ], [ 1, 0, 1, 1, 1, 2, 1, 3 ] ], // I
  [ [ 1, 0, 0, 1, 1, 1, 2, 1 ], [ 1, 0, 1, 1, 2, 1, 1, 2 ], [ 0, 1, 1, 1, 2, 1, 1, 2 ], [ 1, 0, 0, 1, 1, 1, 1, 2 ] ], // T
  [ [ 2, 0, 0, 1, 1, 1, 2, 1 ], [ 1, 0, 1, 1, 1, 2, 2, 2 ], [ 0, 1, 1, 1, 2, 1, 0, 2 ], [ 0, 0, 1, 0, 1, 1, 1, 2 ] ], // L
  [ [ 0, 0, 0, 1, 1, 1, 2, 1 ], [ 1, 0, 2, 0, 1, 1, 1, 2 ], [ 0, 1, 1, 1, 2, 1, 2, 2 ], [ 1, 0, 1, 1, 0, 2, 1, 2 ] ], // J
  [ [ 1, 0, 2, 0, 0, 1, 1, 1 ], [ 1, 0, 1, 1, 2, 1, 2, 2 ], [ 1, 1, 2, 1, 0, 2, 1, 2 ], [ 0, 0, 0, 1, 1, 1, 1, 2 ] ], // S
  [ [ 0, 0, 1, 0, 1, 1, 2, 1 ], [ 2, 0, 1, 1, 2, 1, 1, 2 ], [ 0, 1, 1, 1, 1, 2, 2, 2 ], [ 1, 0, 0, 1, 1, 1, 0, 2 ] ], // Z
  [ [ 1, 0, 2, 0, 1, 1, 2, 1 ], [ 1, 0, 2, 0, 1, 1, 2, 1 ], [ 1, 0, 2, 0, 1, 1, 2, 1 ], [ 1, 0, 2, 0, 1, 1, 2, 1 ] ], // O
];

console.log(process.argv[2]);

const piece_to_char = [ 'I', 'T', 'L', 'J', 'S', 'Z', 'O' ];
let str = `
#pragma push_macro("X")
#define X 255 // Value that would break the program if read and treated seriously. Meant to.
const struct {
  u8 leftbottomright[3][4][4]; // Four rotation states, each with four values for each block
  u8 top_limit[4]; u8 bottom_limit[4]; u8 left_limit[4]; u8 right_limit[4];
} tris_pieces_left_right_bottom[] = { {},\n`;
for(let y = 0; y < tris_pieces_places.length; y++) {
  const piece = tris_pieces_places[y];
  let left = "";
  let right = "";
  let bottom = "";
  
  let leftmaxes = "";
  let rightmaxes = "";
  let bottommaxes = "";
  let topmaxes = "";

  for(let x = 0; x < 4; x++) { // iterate through all rotations (4)
    let piecexmaxes = [-1, -1, -1, -1];
    let pieceymaxes = [-1, -1, -1, -1];
    let piecexmins = [5, 5, 5, 5];
    let pieceymins = [5, 5, 5, 5];

    for(let z = 0; z < 8; z += 2) {
      const blockx = piece[x][z];
      const blocky = piece[x][z + 1];
      piecexmaxes[blocky] = Math.max(piecexmaxes[blocky], blockx);
      pieceymaxes[blockx] = Math.max(pieceymaxes[blockx], blocky);
      piecexmins[blocky] = Math.min(piecexmins[blocky], blockx);
      pieceymins[blockx] = Math.min(pieceymins[blockx], blocky);
    }

    for(let z = 0; z < 4; z ++) {
      let comma = z === 3 && x === 3 ? "" : ",";
      left += (piecexmins[z] === 5 ? "X" : piecexmins[z]) + comma;
      right += (piecexmaxes[z] === -1 ? "X" : piecexmaxes[z]) + comma;
      bottom += (pieceymaxes[z] === -1 ? "X" : pieceymaxes[z]) + comma;
    }
    
    let comma = "";
    if(x !== 3) {
      comma = ","
      left += " ";
      right += " ";
      bottom += " ";
    }

    leftmaxes += Math.min(...piecexmins) + comma;
    rightmaxes += Math.max(...piecexmaxes) + comma;
    bottommaxes += Math.max(...pieceymaxes) + comma;
    topmaxes += Math.min(...pieceymins) + comma;
  }
  str += `  { { ${left},   ${bottom},   ${right} }, { ${topmaxes} }, { ${bottommaxes} }, { ${leftmaxes} }, { ${rightmaxes} } }, // ${piece_to_char[y]}\n`;
}

str += `};
#pragma pop_macro("X")`

console.log(str);
