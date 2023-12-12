var vert = `uniform sampler2D uAtlasSampler;
uniform vec2 uTexelSize;
uniform vec2 uPositionMul;
uniform vec2 uPositionAdd;
uniform vec2 uCanvasSize;

attribute vec2 aPosition;
attribute vec2 aCurvesMin;
attribute vec4 aColor;

varying vec2 vCurvesMin;
varying vec2 vGridMin;
varying vec2 vGridSize;
varying vec2 vNormCoord;
varying vec4 vColor;

#define kGlyphExpandFactor 0.1

// Get non-normalised number in range 0-65535 from two channels of a texel
float ushortFromVec2(vec2 v) {
	// v.x holds most significant bits, v.y holds least significant bits
	return 65280.0 * v.x + 255.0 * v.y;
}

vec2 fetchVec2(vec2 coord) {
	vec2 ret;
	vec4 tex = texture2D(uAtlasSampler, (coord + 0.5) * uTexelSize);

	ret.x = ushortFromVec2(tex.rg);
	ret.y = ushortFromVec2(tex.ba);
	return ret;
}

void decodeUnsignedShortWithFlag(vec2 f, out vec2 x, out vec2 flag) {
	x = floor(f * 0.5);
	flag = mod(f, 2.0);
}

void main() {

	vColor = aColor;

	decodeUnsignedShortWithFlag(aCurvesMin, vCurvesMin, vNormCoord);
	vGridMin = fetchVec2(vCurvesMin);
	vGridSize = fetchVec2(vec2(vCurvesMin.x + 1.0, vCurvesMin.y));

	// Adjust vNormCoord to compensate for expanded glyph bounding boxes
	vNormCoord = vNormCoord * (1.0 + 2.0 * kGlyphExpandFactor) - kGlyphExpandFactor;

	// Transform position
	vec2 pos = aPosition;
	pos.y = 1.0 - pos.y;
	pos = pos * uPositionMul + uPositionAdd;

	gl_Position = vec4(pos, 0.0, 1.0);
	gl_Position.x *= uCanvasSize.y / uCanvasSize.x;

}`;
var frag = `#extension GL_OES_standard_derivatives : enable
precision highp float;

#define numSS 4
#define pi 3.1415926535897932384626433832795
#define kPixelWindowSize 1.0

uniform sampler2D uAtlasSampler;
uniform vec2 uTexelSize;
uniform int uShowGrids;

varying vec2 vCurvesMin;
varying vec2 vGridMin;
varying vec2 vGridSize;
varying vec2 vNormCoord;
varying vec4 vColor;


float positionAt(float p0, float p1, float p2, float t) {
    float mt = 1.0 - t;
    return mt*mt*p0 + 2.0*t*mt*p1 + t*t*p2;
}

float tangentAt(float p0, float p1, float p2, float t) {
    return 2.0 * (1.0-t) * (p1 - p0) + 2.0 * t * (p2 - p1);
}

bool almostEqual(float a, float b) {
    return abs(a-b) < 1e-5;
}

float normalizedUshortFromVec2(vec2 v) {
    // produces value in (0,1) range from a vec2
    // vec2 is assumed to come from two unsigned bytes, where v.x is the most significant byte and v.y is the least significant

    // equivalent to this:
    // return (v.x * 65280.0 + vec2.y * 255.0) / 65535.0;

    return (256.0/257.0) * v.x + (1.0/257.0) * v.y;

}    

vec2 fetchVec2(vec2 coord) {
    vec2 ret;
    vec4 tex = texture2D(uAtlasSampler, (coord + 0.5) * uTexelSize);
    ret.x = normalizedUshortFromVec2(tex.rg);
    ret.y = normalizedUshortFromVec2(tex.ba);
    return ret;
}

void fetchBezier(int coordIndex, out vec2 p[3]) {
    for (int i=0; i<3; i++) {
        p[i] = fetchVec2(vec2(vCurvesMin.x + float(coordIndex + i), vCurvesMin.y)) - vNormCoord;
    }
}

int getAxisIntersections(float p0, float p1, float p2, out vec2 t) {
    if (almostEqual(p0, 2.0*p1 - p2)) {
        t[0] = 0.5 * (p2 - 2.0*p1) / (p2 - p1);
        return 1;
    }

    float sqrtTerm = p1*p1 - p0*p2;
    if (sqrtTerm < 0.0) return 0;
    sqrtTerm = sqrt(sqrtTerm);
    float denom = p0 - 2.0*p1 + p2;
    t[0] = (p0 - p1 + sqrtTerm) / denom;
    t[1] = (p0 - p1 - sqrtTerm) / denom;
    return 2;
}

float integrateWindow(float x) {
    float xsq = x*x;
    return sign(x) * (0.5 * xsq*xsq - xsq) + 0.5;           // parabolic window
    //return 0.5 * (1.0 - sign(x) * xsq);                     // box window
}

mat2 getUnitLineMatrix(vec2 b1, vec2 b2) {
    vec2 V = b2 - b1;
    float normV = length(V);
    V = V / (normV*normV);

    return mat2(V.x, -V.y, V.y, V.x);
}

void updateClosestCrossing(in vec2 p[3], mat2 M, inout float closest) {

    for (int i=0; i<3; i++) {
        p[i] = M * p[i];
    }

    vec2 t;
    int numT = getAxisIntersections(p[0].y, p[1].y, p[2].y, t);

    for (int i=0; i<2; i++) {
        if (i == numT) break;
        if (t[i] > 0.0 && t[i] < 1.0) {
            float posx = positionAt(p[0].x, p[1].x, p[2].x, t[i]);
            if (posx > 0.0 && posx < abs(closest)) {
                float derivy = tangentAt(p[0].y, p[1].y, p[2].y, t[i]);
                closest = (derivy < 0.0) ? -posx : posx;
            }
        }
    }
}

mat2 inverse(mat2 m) {
  return mat2(m[1][1],-m[0][1],
             -m[1][0], m[0][0]) / (m[0][0]*m[1][1] - m[0][1]*m[1][0]);
}


void main() {
    vec2 integerCell = floor( clamp(vNormCoord * vGridSize, vec2(0.5), vec2(vGridSize)-0.5));
    vec2 indicesCoord = vGridMin + integerCell + 0.5;
    vec2 cellMid = (integerCell + 0.5) / vGridSize;

    mat2 initrot = inverse(mat2(dFdx(vNormCoord) * kPixelWindowSize, dFdy(vNormCoord) * kPixelWindowSize));

    float theta = pi/float(numSS);
    mat2 rotM = mat2(cos(theta), sin(theta), -sin(theta), cos(theta));      // note this is column major ordering

    ivec4 indices1, indices2;
    indices1 = ivec4(texture2D(uAtlasSampler, indicesCoord * uTexelSize) * 255.0 + 0.5);
    indices2 = ivec4(texture2D(uAtlasSampler, vec2(indicesCoord.x + vGridSize.x, indicesCoord.y) * uTexelSize) * 255.0 + 0.5);

    bool moreThanFourIndices = indices1[0] < indices1[1];

    float midClosest = (indices1[2] < indices1[3]) ? -2.0 : 2.0;

    float firstIntersection[numSS];
    for (int ss=0; ss<numSS; ss++) {
        firstIntersection[ss] = 2.0;
    }

    float percent = 0.0;

    mat2 midTransform = getUnitLineMatrix(vNormCoord, cellMid);

    for (int bezierIndex=0; bezierIndex<8; bezierIndex++) {
        int coordIndex;

        if (bezierIndex < 4) {
            coordIndex = indices1[bezierIndex];
        } else {
            if (!moreThanFourIndices) break;
            coordIndex = indices2[bezierIndex-4];
        }

        if (coordIndex < 2) {
            continue;
        }

        vec2 p[3];
        fetchBezier(coordIndex, p);

        updateClosestCrossing(p, midTransform, midClosest);

        // Transform p so fragment in glyph space is a unit circle
        for (int i=0; i<3; i++) {
            p[i] = initrot * p[i];
        }


        // Iterate through angles
        for (int ss=0; ss<numSS; ss++) {
            vec2 t;
            int numT = getAxisIntersections(p[0].x, p[1].x, p[2].x, t);

            for (int tindex=0; tindex<2; tindex++) {
                if (tindex == numT) break;

                if (t[tindex] > 0.0 && t[tindex] <= 1.0) {

                    float derivx = tangentAt(p[0].x, p[1].x, p[2].x, t[tindex]);
                    float posy = positionAt(p[0].y, p[1].y, p[2].y, t[tindex]);

                    if (posy > -1.0 && posy < 1.0) {
                        // Note: whether to add or subtract in the next statement is determined
                        // by which convention the path uses: moving from the bezier start to end, 
                        // is the inside to the right or left?
                        // The wrong operation will give buggy looking results, not a simple inverse.
                        float delta = integrateWindow(posy);
                        percent = percent + (derivx < 0.0 ? delta : -delta);

                        float intersectDist = posy + 1.0;
                        if (intersectDist < abs(firstIntersection[ss])) {
                            firstIntersection[ss] = derivx < 0.0 ? -intersectDist : intersectDist;
                        }
                    }
                }
            }

            if (ss+1<numSS) {
                for (int i=0; i<3; i++) {
                    p[i] = rotM * p[i];
                }
            }
        }   // ss
        

    }

    bool midVal = midClosest < 0.0;

    // Add contribution from rays that started inside
    for (int ss=0; ss<numSS; ss++) {
        if ((firstIntersection[ss] >= 2.0 && midVal) || (firstIntersection[ss] > 0.0 && abs(firstIntersection[ss]) < 2.0)) {
            percent = percent + 1.0 /*integrateWindow(-1.0)*/;
        }
    }

    percent = percent / float(numSS);

    //percent = (midClosest > 0.0) ? 0.0 : 1.0;

    gl_FragColor = vColor;
    gl_FragColor.a *= percent;
    
    if (uShowGrids != 0) {
        vec2 gridCell = mod(floor(integerCell), 2.0);
        gl_FragColor.r = (gridCell.x - gridCell.y) * (gridCell.x - gridCell.y);
        gl_FragColor.a += 0.3;
    }

}`

var canvas;
var gl;
var glext;
var timerQuery, waitingForTimer, lastFrametime;
var vertexShader, fragmentShader;
var glyphProgram, pageProgram, imageProgram;
var atlasTexture;
var glyphBuffer;
var pageBuffer;
var pageData;
var vertexCount = 0;
var indexCount = 0;
var int16PerVertex = 6;	// const
var renderRequired = false;

var transform = {
	x: 0.5,
	y: 0.5,
	zoom: 0
}

var animTransform = {
	x: 0,
	y: 0,
	zoom: 1
}

var lastAnimationTimestamp;
var animationDuration = 60;

function mix(b, a, t) {
	if (t < 0) t = 0;
	else if (t > 1) t = 1;
	
	return a * t + b * (1 - t);
}

function log(s) {
	console.log(s);
	document.getElementById("loadinginfo").textContent += s + "\n";
}

function significantChange(a, b) {
	if (Math.abs(b) < Math.abs(a)) {
		var t = b;
		b = a;
		a = t;
	}
	if (b == 0) return false;
	return (a/b) < 0.99999999
}

function getAnimatedValue(value, target, elapsed) {
		return mix(value, target, elapsed / animationDuration);
}

function updateAnimations(timestamp) {
	var elapsed = lastAnimationTimestamp ? timestamp - lastAnimationTimestamp : 0;
	
	var changed = false;
	for (var key in animTransform) {
		var newval = getAnimatedValue(animTransform[key], transform[key], elapsed);
		if (significantChange(newval, animTransform[key])) {
			changed = true;
		}
		animTransform[key] = newval;
	}
	
	lastAnimationTimestamp = timestamp;

	return changed;
}

function finishAnimations() {
	for (var key in animTransform) {
		animTransform[key] = transform[key];
	}
	forceRender();
}

function forceRender() {
	renderRequired = true;
}

function initGl() {
	// need alpha: false so what's behind the webgl canvas doesn't bleed through
	// see http://www.zovirl.com/2012/08/24/webgl_alpha/
	gl = canvas.getContext("webgl", { alpha: false});
	if (gl == null) {
		gl = canvas.getContext("experimental-webgl", { alpha: false});
		if (gl == null) {
			log("Failed to create WebGL context");
			return;
		}
	}

			if (gl.getExtension('OES_standard_derivatives') == null) {
					log("Failed to enable OES_standard_derivatives");
					return;
			}

			glext = gl.getExtension('EXT_disjoint_timer_query');
			if (glext) {
				timerQuery = glext.createQueryEXT();
				document.getElementById("frametime").style.display = "inline";
			}

	gl.disable(gl.DEPTH_TEST);

	gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);   

	gl.viewport(0, 0, canvas.width, canvas.height);
	gl.clearColor(160/255, 169/255, 175/255, 1.0);
}

	function compileShader(str, shaderType) {
	var shader;
	if (shaderType == "frag") {
		shader = gl.createShader(gl.FRAGMENT_SHADER);
	} else if (shaderType == "vert") {
		shader = gl.createShader(gl.VERTEX_SHADER);
	} else {
		log("Unknown shader type: " + shaderType);
		return null;
	}
	
	gl.shaderSource(shader, str);
	gl.compileShader(shader);
	if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
		log("Failed to compile " + shaderType + " shader:\n" + gl.getShaderInfoLog(shader));
		//console.log("Shader source:\n" + str);
		return null;
	}
	return shader;
	}	

function getShader(id) {
	var elem = document.getElementById(id);
	if (!elem) {
		log("Can't find shader element " + id);
		return null;
	}
	var shaderType;
	if (elem.type == "x-shader/x-vertex") {
		shaderType = "vert";
	} else if (elem.type == "x-shader/x-fragment") {
		shaderType = "frag";
	} else {
		log("getShader: unknown shader type in script tag for id " + id);
	}

	return compileShader(elem.textContent, shaderType);
}

function checkShadersReady() {
	if (fragmentShader && vertexShader) {
		if (glyphProgram) {
			gl.deleteProgram(glyphProgram);
		}
		log("Linking shader program")
		glyphProgram = gl.createProgram();
		gl.attachShader(glyphProgram, vertexShader);
		gl.attachShader(glyphProgram, fragmentShader);
		gl.linkProgram(glyphProgram);
		
		if (!gl.getProgramParameter(glyphProgram, gl.LINK_STATUS)) {
			log("Could not link glyph shader program: " + gl.getProgramInfoLog(program));
			gl.deleteProgram(glyphProgram);
			glyphProgram = null;
			return;
		}
		
		// Attribs
		setupAttribute(glyphProgram, "aPosition");
		setupAttribute(glyphProgram, "aCurvesMin");
		//setupAttribute(glyphProgram, "aNormCoord");
		setupAttribute(glyphProgram, "aColor");

		// Uniforms
		setupUniform(glyphProgram, "uTexelSize");
		setupUniform(glyphProgram, "uPositionMul");
		setupUniform(glyphProgram, "uPositionAdd");
		setupUniform(glyphProgram, "uCanvasSize");
		setupUniform(glyphProgram, "uAtlasSampler");
		setupUniform(glyphProgram, "uShowGrids");

		gl.deleteShader(vertexShader);
		gl.deleteShader(fragmentShader);
		vertexShader = null;
		fragmentShader = null;
	}
}


function initImageShader() {
	var frag = getShader("imagefs");
	var vert = getShader("imagevs");
	imageProgram = gl.createProgram();
	gl.attachShader(imageProgram, frag);
	gl.attachShader(imageProgram, vert);
	gl.linkProgram(imageProgram);
	
	if (!gl.getProgramParameter(imageProgram, gl.LINK_STATUS)) {
		log("Could not link image shader program" + gl.programInfoLog(imageProgram));
		gl.deleteProgram(imageProgram);
		return;
	}
	
	// Attribs
	setupAttribute(imageProgram, "aPosition");
	setupAttribute(imageProgram, "aAlpha");
	setupAttribute(imageProgram, "aWhichVertex");

	// Uniforms
	setupUniform(imageProgram, "uPositionMul");
	setupUniform(imageProgram, "uPositionAdd");
	setupUniform(imageProgram, "uSampler");

}

function initPageShader() {
	var frag = getShader("pagefs");
	var vert = getShader("pagevs");
	pageProgram = gl.createProgram();
	gl.attachShader(pageProgram, frag);
	gl.attachShader(pageProgram, vert);
	gl.linkProgram(pageProgram);
	
	if (!gl.getProgramParameter(pageProgram, gl.LINK_STATUS)) {
		log("Could not link page shader program" + gl.programInfoLog(pageProgram));
		gl.deleteProgram(pageProgram);
		return;
	}
	
	gl.useProgram(pageProgram);
	
	// Attribs
	setupAttribute(pageProgram, "aPosition");

	// Uniforms
	setupUniform(pageProgram, "uPositionMul");
	setupUniform(pageProgram, "uPositionAdd");
	setupUniform(pageProgram, "uCanvasSize");

	pageBuffer = gl.createBuffer();
	gl.useProgram(null);
}

	function processFragmentShader() {
		fragmentShader = compileShader(this.responseText, 'frag');
		checkShadersReady();
	}

	function processVertexShader() {
		vertexShader = compileShader(this.responseText, 'vert');
		checkShadersReady();
	}

function processPageData() {
	pageData = JSON.parse(this.responseText);
	log("Loaded " + pageData.length + " page(s)")
	computePageLocations();
	var pageVerts = new Float32Array(pageData.length * 6 * 2);

	for (var i=0; i<pageData.length; i++) {
		var j = i*6*2;
		var x0 = -pageData[i].x;
		var y0 = pageData[i].y;
		var x1 = x0 + pageData[i].width / pageData[0].width;
		var y1 = y0 + pageData[i].height / pageData[0].height;
		pageVerts[j+ 0] = x0;
		pageVerts[j+ 1] = y0;
		pageVerts[j+ 2] = x0;
		pageVerts[j+ 3] = y0;
		pageVerts[j+ 4] = x1;
		pageVerts[j+ 5] = y0;
		pageVerts[j+ 6] = x0;
		pageVerts[j+ 7] = y1;
		pageVerts[j+ 8] = x1;
		pageVerts[j+ 9] = y1;
		pageVerts[j+10] = x1;
		pageVerts[j+11] = y1;
	}
	gl.bindBuffer(gl.ARRAY_BUFFER, pageBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, pageVerts, gl.STATIC_DRAW);
}

function requestFile(filename, cb, rtype) {
			var req = new XMLHttpRequest();
			req.open("GET", uncached(filename), true);
			req.onload = cb;
			if (rtype) {
				req.responseType = rtype;
			}
			req.send(null);
}

function setupAttribute(prog, name) {
	if (prog.attributes == null) {
		prog.attributes = {};
	}
	var loc = gl.getAttribLocation(prog, name);
	if (loc == -1) {
		log("Failed to get location for attribute " + name);
		return;
	}
	prog.attributes[name] = loc;
}

function enableAttributes(prog) {
	for (var a in prog.attributes) {
		gl.enableVertexAttribArray(prog.attributes[a])
	}
}

function disableAttributes(prog) {
	for (var a in prog.attributes) {
		gl.disableVertexAttribArray(prog.attributes[a]);
	}
}

function setupUniform(prog, name) {
	if (prog.uniforms == null) {
		prog.uniforms = {};
	}

	var loc = gl.getUniformLocation(prog, name);
	if (loc == -1) {
		log("Failed to get location for uniform " + name);
		return;
	}
	prog.uniforms[name] = loc;		
}

function unpackBmp(buf) {
	// TODO: endian issues
	var iarr = new Uint16Array(buf, 18, 4);
	return {buf: buf.slice(54), width: iarr[0], height: iarr[2]};
}

function processAtlas() {
	var data = unpackBmp(this.response);
	var arrayView = new Uint8Array(data.buf);
	atlasTexture = gl.createTexture();
	atlasTexture.width = data.width;
	atlasTexture.height = data.height;
	gl.bindTexture(gl.TEXTURE_2D, atlasTexture);
	gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
	gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
	//gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, atlasTexture.width, atlasTexture.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, arrayView);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
	gl.bindTexture(gl.TEXTURE_2D, null);
	log("Loaded atlas: " + atlasTexture.width + " x " + atlasTexture.height);
}

function copyVertex(dstArray, dstIndex, srcArray, srcIndex) {
	// dstAray and srcArray should both be Int16Array 
	for (var j=0; j<int16PerVertex; j++) {
		dstArray[dstIndex*int16PerVertex+j] = srcArray[srcIndex*int16PerVertex+j];
	}
}

function transposeBytes(buf, innerDim) {
	var inputArray = new Uint8Array(buf);
	var outputArray = new Uint8Array(inputArray.length);

	var outerDim = inputArray.length / innerDim;

	for (var i=0; i<innerDim; i++) {
		for (var j=0; j<outerDim; j++) {
			outputArray[j * innerDim + i] = inputArray[i * outerDim + j];
		}
	}

	return outputArray.buffer;
}

function transposeBuffer(buf, innerDim) {
	var inputArray = new Uint16Array(buf);
	var outputArray = new Uint16Array(inputArray.length);

	var outerDim = inputArray.length / innerDim;

	for (var i=0; i<innerDim; i++) {
		for (var j=0; j<outerDim; j++) {
			outputArray[j * innerDim + i] = inputArray[i * outerDim + j];
		}
	}

	return outputArray.buffer;
}

var positions = {x:[], y:[]};		// for auto zoom

function deltaDecodePositions(buf) {
	var p = new Int16Array(buf);
	var q = new Uint16Array(buf);

	positions.x[0] = q[0]/65535.0;
	positions.y[0] = q[numVerts]/65535.0;
	var numVerts = p.length / int16PerVertex;
	for (var i=1; i<numVerts; i++) {
		p[i] += p[i-1];
		p[i+numVerts] += p[i+numVerts-1];
		positions.x[i] = q[i]/65535.0;
		positions.y[i] = q[i+numVerts]/65535.0;
	}
}

function boxesIntersect(a, b) {
	return a.x0 < b.x1 && a.y0 < b.y1 && a.x1 > b.x0 && a.y1 > b.y0;
}

function processVertexResponse() {
	var buf = unpackBmp(this.response).buf;
	deltaDecodePositions(buf);
	var vertBuffer = transposeBuffer(buf, int16PerVertex);

	var int16Array = new Int16Array(vertBuffer);

	log('Loaded ' + (int16Array.length / int16PerVertex / 4) + ' glyphs');

	vertexCount = int16Array.length / int16PerVertex;
	
	// Convert vertex data to tri strip of quads with 1st and 4th vertex repeated
	var stripVertBuffer = new ArrayBuffer(vertBuffer.byteLength / 4 * 6);		// 4 verts per quad gets expanded from 4 to 6 with degen verts
	var stripArray = new Int16Array(stripVertBuffer);
	
	for (var quadIndex = 0; quadIndex < vertexCount / 4; quadIndex++) {
		var i = quadIndex * 4;		// input:  4 vertices per quad
		var o = quadIndex * 6;		// output: 6 vertices per quad
		
		// Copy degenerate first vertex of quad
		copyVertex(stripArray, o, int16Array, i);
		
		// Copy first 4 verts of quad
		copyVertex(stripArray, o+1, int16Array, i+0);
		copyVertex(stripArray, o+2, int16Array, i+1);
		copyVertex(stripArray, o+3, int16Array, i+2);
		copyVertex(stripArray, o+4, int16Array, i+3);
		
		// Repeat 4th vertex in output
		copyVertex(stripArray, o+5, int16Array, i+3);
	}
	
	
	glyphBuffer = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, glyphBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, stripArray, gl.STATIC_DRAW);

	// Update vert count with expanded vertex count
	vertexCount = stripArray.length / int16PerVertex;
}

function processImageVertices() {
	var buf = unpackBmp(this.response).buf;
	imageBuffer = gl.createBuffer();
	gl.bindBuffer(imageBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, buf, gl.STATIC_DRAW);
	console.log("Loaded image vertex buffer");
}

function setCanvasSize() {
	//return;
	var devicePixelRatio = window.devicePixelRatio || 1;
	//var w = Math.round(canvas.clientWidth * devicePixelRatio);
	//var h = Math.round(canvas.clientHeight * devicePixelRatio);
	var e = document.getElementById("canvaswrap");
	var w = Math.round(e.clientWidth * devicePixelRatio);
	var h = Math.round(e.clientHeight * devicePixelRatio);

	if (canvas.width != w || canvas.height != h) {
		canvas.width = w;
		canvas.height = h;
	}		
}

function drawPage(page, x, y, zoomx, zoomy) {
	// Shader would have done:
	//pos = (pos - uTranslate) / uZoom;
	//So, pos * (1/uZoom) + (-uTranslate/uZoom);
	var translateX = page.x + x;
	var translateY = page.y + y;

	var pageNdc = {
		x0: (0 - translateX) / zoomx * canvas.height / canvas.width,
		x1: (1 - translateX) / zoomx * canvas.height / canvas.width,
		y0: (0 - translateY) / zoomy,
		y1: (1 - translateY) / zoomy,
	}
	var viewportNdc = {
		x0: -1,
		x1:  1,
		y0: -1,
		y1:  1,
	}
	if (!boxesIntersect(pageNdc, viewportNdc)) {
		return;
	}


	gl.uniform2f(glyphProgram.uniforms.uPositionMul, 1/zoomx, 1/zoomy);
	gl.uniform2f(glyphProgram.uniforms.uPositionAdd, -translateX / zoomx, -translateY / zoomy);
	gl.drawArrays(gl.TRIANGLE_STRIP, page.beginVertex / 4 * 6, (page.endVertex - page.beginVertex) / 4 * 6);
}

function computePageLocations() {
	var cols = Math.floor(Math.sqrt(pageData.length / canvas.height * canvas.width / pageData[0].width * pageData[0].height));

	for (var i=0; i<pageData.length; i++) {
		var page = pageData[i];
		page.x = -(i % cols);
		page.y = Math.floor(i / cols);

		var gap = 1.02;
		page.x *= gap;
		page.y *= gap;
	}		

}

var lastAutoChange = -1e6;

function drawScene(timestamp) {

	if (glyphProgram == null || !vertexCount || !pageData || !atlasTexture) {
		return;
	}
	var firstFrame = document.getElementById("loadinginfo").style.visibility != "hidden";
	if (firstFrame) {
		document.getElementById("loadinginfo").style.visibility = "hidden";
		canvas.style.display = "block";	// force reflow on ios
	}

	var zoomx = Math.pow(2, animTransform.zoom);
	var zoomy = zoomx * pageData[0].width / pageData[0].height;

	var autoPan = document.getElementById("autopan").checked;
	if (autoPan) {
		var interval = 8000;
		if (timestamp - lastAutoChange > interval) {
			lastAutoChange = timestamp;
			var page = pageData[Math.floor(Math.random() * pageData.length)];
			var glyph = Math.floor(Math.random() * (page.endVertex - page.beginVertex)) + page.beginVertex;
			glyph = Math.floor(glyph/4)*4;
			var x = 0.5 * (positions.x[glyph] + positions.x[glyph+3]);
			var y = 0.5 * (positions.y[glyph] + positions.y[glyph+3]);

			transform.x = -page.x + x;
			transform.y = -page.y + 1.0 - y;
		}
		var t = (timestamp - lastAutoChange) / interval;
		var steps = 11;
		//t = Math.min(1, steps/(steps-1)*t);
		//t = Math.floor(t*steps)/steps;
		transform.zoom = 0.5-6*t;
		transform.zoom = Math.cos(Math.acos(0)*t*4);
		transform.zoom = Math.pow(transform.zoom+1, 3)/1.1-7;
	}

	if (!updateAnimations(timestamp) && !firstFrame && !renderRequired) {
		return;
	}
	renderRequired = false;

	setCanvasSize();
	gl.viewport(0, 0, canvas.width, canvas.height);
	gl.clear(gl.COLOR_BUFFER_BIT);

	if (timerQuery) {
		if (waitingForTimer) {
			var available = glext.getQueryObjectEXT(timerQuery, glext.QUERY_RESULT_AVAILABLE_EXT);
			var disjoint = gl.getParameter(glext.GPU_DISJOINT_EXT);
			if (available) {
				if (lastFrametime == null || timestamp - lastFrametime > 100) {
					lastFrametime = timestamp;
					var elapsed = glext.getQueryObjectEXT(timerQuery, glext.QUERY_RESULT_EXT);
					document.getElementById("frametime").value = elapsed/1e6;
				}
				waitingForTimer = false;
			}
		}

		if (!waitingForTimer) {
			glext.beginQueryEXT(glext.TIME_ELAPSED_EXT, timerQuery);
		}
	}


	// Draw page backgrounds
	gl.useProgram(pageProgram);
	gl.disable(gl.BLEND);
	gl.uniform2f(pageProgram.uniforms.uCanvasSize, canvas.width, canvas.height);
	gl.uniform2f(pageProgram.uniforms.uPositionMul, 1/zoomx, 1/zoomy);
	gl.uniform2f(pageProgram.uniforms.uPositionAdd, -animTransform.x / zoomx, -animTransform.y / zoomy);
	gl.bindBuffer(gl.ARRAY_BUFFER, pageBuffer);
	enableAttributes(pageProgram);
	gl.vertexAttribPointer(pageProgram.attributes.aPosition, 2, gl.FLOAT, false, 0, 0);
	gl.drawArrays(gl.TRIANGLE_STRIP, 0, pageData.length * 6);
	disableAttributes(pageProgram);

	// Draw glyphs
	gl.useProgram(glyphProgram);
	gl.bindBuffer(gl.ARRAY_BUFFER, glyphBuffer);
	gl.enable(gl.BLEND);
	
	enableAttributes(glyphProgram);
	gl.vertexAttribPointer(glyphProgram.attributes.aPosition,  2, gl.UNSIGNED_SHORT, true, int16PerVertex*2,  0);
	gl.vertexAttribPointer(glyphProgram.attributes.aCurvesMin, 2, gl.UNSIGNED_SHORT, false, int16PerVertex*2, 2*2);
	gl.vertexAttribPointer(glyphProgram.attributes.aColor,     4, gl.UNSIGNED_BYTE,  true, int16PerVertex*2,  4*2);
	
	gl.activeTexture(gl.TEXTURE0);
	gl.bindTexture(gl.TEXTURE_2D, atlasTexture);
	gl.uniform1i(glyphProgram.uniforms.uAtlasSampler, 0);
		
	gl.uniform2f(glyphProgram.uniforms.uTexelSize, 1/atlasTexture.width, 1/atlasTexture.height);
	gl.uniform2f(glyphProgram.uniforms.uCanvasSize, canvas.width, canvas.height);
	gl.uniform1i(glyphProgram.uniforms.uShowGrids, document.getElementById("showgrids").checked ? 1 : 0);

	// Draw page contents
	for (var i=0; i<pageData.length; i++) {
		drawPage(pageData[i], animTransform.x, animTransform.y, zoomx, zoomy);
	}		
	disableAttributes(glyphProgram);

	if (timerQuery && !waitingForTimer) {
		glext.endQueryEXT(glext.TIME_ELAPSED_EXT);
		waitingForTimer = true;
	}
}

function tick(timestamp) {
	requestAnimationFrame(tick);
	drawScene(timestamp);
}

function canvasMouseEnter(e) {
	this.lastX = e.clientX;
	this.lastY = e.clientY;
}

	
var prevPinchDiff = -1;
function canvasTouchStart(e) {
	var touch = e.targetTouches[0];
	this.lastX = touch.clientX;
	this.lastY = touch.clientY;
	this.primaryTouchId = touch.identifier;
	prevPinchDiff = -1;
}

function handlePinch(ev) {
	var touches = ev.targetTouches;

	if (touches.length == 2) {
			var dx = touches[0].clientX - touches[1].clientX;
			var dy = touches[0].clientY - touches[1].clientY;

			var curDiff = Math.sqrt(dx*dx + dy*dy);

			if (prevPinchDiff > 0) {
			transform.zoom -= (curDiff - prevPinchDiff) / 80;
			finishAnimations();
		}
		prevPinchDiff = curDiff;
	}
}


function canvasTouchMove(e) {
	var touch = e.targetTouches[0];


	if (e.targetTouches.length > 1) {
		handlePinch(e);
	} else {
		if (this.primaryTouchId != touch.identifier) {
			this.lastX = touch.clientX;
			this.lastY = touch.clientY;
			this.primaryTouchId = touch.identifier;
		}
	
		if (e.altKey) {
			var dx = touch.clientX - this.lastX;
			var dy = touch.clientY - this.lastY;
			var dmax = Math.abs(dx) > Math.abs(dy) ? -dx : dy;
			transform.zoom += 7.0 * (dmax / this.offsetHeight);	
		} else {
			var scaleFactor = 4;
			transform.x -= scaleFactor * ((touch.clientX - this.lastX) / this.offsetWidth) * Math.pow(2, transform.zoom);
			transform.y += scaleFactor * ((touch.clientY - this.lastY) / this.offsetHeight) * Math.pow(2, transform.zoom);
		}
		
		finishAnimations();
	}

	this.lastX = touch.clientX;
	this.lastY = touch.clientY;

	e.preventDefault();
}

function canvasMouseMove(e) {
	if (e.buttons) {
		if (e.altKey || e.button == 2 || e.buttons == 2) {
			var dx = e.clientX - this.lastX;
			var dy = e.clientY - this.lastY;
			var dmax = Math.abs(dx) > Math.abs(dy) ? -dx : dy;
			transform.zoom += 7.0 * (dmax / this.offsetHeight);	
		} else {
			var scaleFactor = 5.5;
			transform.x -= scaleFactor * ((e.clientX - this.lastX) / this.offsetWidth) * Math.pow(2, transform.zoom);
			transform.y += scaleFactor * ((e.clientY - this.lastY) / this.offsetHeight) * Math.pow(2, transform.zoom);
		}
	}

	this.lastX = e.clientX;
	this.lastY = e.clientY;
}

function canvasMouseWheel(e) {
	var amt = e.deltaY;
	if (e.deltaMode == 1) amt *= 30;
	if (e.deltaMode == 2) amt *= 1000;
	transform.zoom += amt / 200;
	e.preventDefault();
}


function fullscreen() {
	var e = document.getElementById("canvaswrap")
	if (e.requestFullscreen) {
		e.requestFullscreen();
	} else if (e.webkitRequestFullScreen) {
		e.webkitRequestFullScreen();
	} else if (e.mozRequestFullScreen) {
		e.mozRequestFullScreen();
	} else if (canvas.msRequestFullscreen) {
		e.msRequestFullscreen();
	}

	forceRender();
}

function uncached(s) {
	if (document.location.hostname == "localhost") {
		return s + "?" + Math.random();
	}
	return s;
}

//var lzvert = [];

function initShaders() {
	requestFile('font.frag', processFragmentShader);
	requestFile('web.vert', processVertexShader);
}

function webGlStart() {
	if (window.navigator.userAgent.indexOf("Trident") >= 0 || window.navigator.userAgent.indexOf("Edge") >= 0) {
		log("The shader is currently very slow to compile on Microsoft browsers. Hold tight..");
	}

	canvas = document.getElementById("beziercanvas");
	canvas.addEventListener("touchmove", canvasTouchMove);
	canvas.addEventListener("touchstart", canvasTouchStart);
	canvas.addEventListener("mousemove", canvasMouseMove);
	canvas.addEventListener("mouseenter", canvasMouseEnter);
	canvas.addEventListener("wheel", canvasMouseWheel);
	canvas.addEventListener("contextmenu", function(e) {e.preventDefault()}, false);
	
	window.addEventListener("resize", forceRender);

	initGl();
	log("Loading files...");
	requestFile("vertices.bmp", processVertexResponse, "arraybuffer");
	requestFile("atlas.bmp", processAtlas, "arraybuffer");
	requestFile('pages.json', processPageData);
	//requestFile('imagevertices.bmp', processImageVertices, "arraybuffer");
	initShaders();

	//requestFile("vertices.bmp.lzma", function() { console.log("got lzma ", this.response.byteLength); lzvert = this.response}, "arraybuffer");

	initPageShader();
			
	tick();

	if (document.location.hostname == "localhost") {
		document.getElementById("reloadbutton").style.display = "inline-block";
	}
}
