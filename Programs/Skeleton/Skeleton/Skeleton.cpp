//=============================================================================================
// Program: Graf rajzolo
// ---------------------------------------------------------------------------------------------
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Juhasz Benedek Laszlo
// Neptun : C8B5CT
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================

#include "framework.h"

struct Node {
	vec3 pos = vec3(0, 0, 1);
	vec3 velocity = vec3(0, 0, 0);
};

const int N = 50;
const int PERCENT_OF_EDGES = 5;
const int E = N * (N - 1) / 2 * PERCENT_OF_EDGES / 100;

const int START_SCALE = 1;
const int SCALE = 5;
const float OPTIMAL_DISTANCE = 0.05f;
const int RESOLUTION = 20;
const int TEXTURE_SIZE = 64;
const float RADIUS = 0.03f;


const int REFRESH_RATE = 8;
const int REFRESH_TIME = 1000 / REFRESH_RATE;
const int ITERATION_PER_FRAME = 40;

const float BASE_FORCE = 1e-4f;
const float GLOBAL_C = 1e1f;
const float FRICTION_C = 1e5f;
const float PULL_C = 1e2f;
const float PUSH_C = 1e1f;

const int TRIES = 100;

float coscache[RESOLUTION + 1];
float sincache[RESOLUTION + 1];
vec2 uvcache[RESOLUTION + 1];

const char* const vertexSource = R"(
	#version 330
	precision highp float;

	uniform mat4 MVP;
	layout(location = 0) in vec3 v;
	layout(location = 1) in vec2 t_in;

	out vec2 t_out;

	void main() {
		gl_Position =  vec4(v.x/v.z, v.y/v.z, 0, 1) * MVP;
		t_out = t_in;
	}
)";

const char* const fragmentSource = R"(
	#version 330	
	precision highp float;
	
	uniform sampler2D samplerUnit;
	in vec2 t_out;
	out vec4 outColor;	

	void main() {
		outColor = texture(samplerUnit,t_out);
	}
)";

Node nodes[N + 1];

vec4 nodeTexture[N + 1][TEXTURE_SIZE * TEXTURE_SIZE];
vec4 edgeTexture[TEXTURE_SIZE * TEXTURE_SIZE];

unsigned vao;
unsigned vbo[2];
unsigned nodeTextureId[N + 1];
unsigned edgeTextureId;

bool edges[N][N]{ false };

GPUProgram gpuProgram;

vec2 mouseBase;
bool runs = false;
int lastIterationTime = 0;

float distance(vec3 const& v1, vec3 const& v2) {
	float operand = -1 * (v1.x * v2.x + v1.y * v2.y - v1.z * v2.z);
	return operand > 1.0f ? acoshf(operand) : 0.0f;
}

float LorentzAbs(vec3 const& v) {
	float operand = v.x * v.x + v.y * v.y - v.z * v.z;
	return operand > 0.0f ? sqrtf(operand) : 0.0f;
}

vec3 direction(vec3 const& v1, vec3 const& v2) {
	float dis = distance(v1, v2);
	return dis > 1e-7 ? (v2 - v1 * coshf(dis)) / sinhf(dis) : vec3(0, 0, 0);
}

void correctZ(vec3& v) {
	v.z = sqrtf(v.x * v.x + v.y * v.y + 1);
}

vec3 globalForce(vec3& pos) {
	float d = distance(pos, nodes[N].pos);
	return BASE_FORCE * GLOBAL_C * d * d * direction(pos, nodes[N].pos);
}

vec3 frictionForce(vec3& velocity) {
	float v = LorentzAbs(velocity);
	return -1 * BASE_FORCE * FRICTION_C * v * velocity;
}

vec3 nodalForce(vec3& pos, int i) {
	vec3 nodalForce;
	for (int j = 0; j < N; j++) {
		if (j == i)
			continue;
		float d = distance(pos, nodes[j].pos);
		if (edges[i][j]) {
			float del = d - OPTIMAL_DISTANCE;
			nodalForce = nodalForce + BASE_FORCE * PULL_C * del * del * del / d * direction(pos, nodes[j].pos);
		}
		else
			nodalForce = nodalForce - BASE_FORCE * PUSH_C / d / d * direction(pos, nodes[j].pos);
	}
	return nodalForce;
}

void heuristic() {
	for (int i = 0; i != N; i++) {
		vec3& pos = nodes[i].pos;
		pos.x = (float(rand()) / RAND_MAX - 0.5f) * SCALE;
		pos.y = (float(rand()) / RAND_MAX - 0.5f) * SCALE;
		correctZ(pos);
		nodes[i].velocity = vec3(0, 0, 0);
	}
	nodes[N].pos = vec3(0, 0, 1);
	for (int t = 0; t != TRIES; t++) {
		for (int i = 0; i != N; i++) {
			vec3& pos = nodes[i].pos;
			vec3 newPos;
			newPos.x = (float(rand()) / RAND_MAX - 0.5f) * SCALE;
			newPos.y = (float(rand()) / RAND_MAX - 0.5f) * SCALE;
			correctZ(newPos);

			if (LorentzAbs(globalForce(pos) + nodalForce(pos, i)) > LorentzAbs(globalForce(newPos) + nodalForce(newPos, i)))
				pos = newPos;
		}
	}
	lastIterationTime = 0;
}

void iterate() {
	for (int i = 0; i != N; i++) {
		vec3& r = nodes[i].pos;
		vec3& v = nodes[i].velocity;
		vec3 f = globalForce(r) + frictionForce(v) + nodalForce(r, i);
		v = v + f;
	}
	for (int i = 0; i != N; i++) {
		vec3& r = nodes[i].pos;
		vec3& v = nodes[i].velocity;
		float dis = LorentzAbs(v);
		if (dis > 1e-5) {
			vec3 oldr = r;
			vec3 newr = r * coshf(dis) + v / sqrtf(dis) * sinhf(dis); //TODO normalize
			correctZ(newr);
			if (newr.x == newr.x && newr.y == newr.y && newr.z == newr.z) {
				r = newr;
				vec3 newdir = -1 * direction(r, oldr);
				v = dis * newdir;
			}
		}
	}
}

vec3 deproject(vec2 const& v) {
	return vec3(v.x, v.y, 1) / sqrtf(1 - v.x * v.x - v.y * v.y);
}

vec3 translateNode(vec3 pos, vec3 const& o) {
	float dis = distance(pos, o);
	if (dis > 1e-5) {
		vec3 dir = direction(pos, o);
		pos = pos * coshf(2 * dis) + dir * sinhf(2 * dis);
		correctZ(pos);
	}
	return pos;
}

void translate(vec2 const& base, vec2 const& end) {
	vec3 p = deproject(base);
	vec3 q = deproject(end);

	vec3 dir = direction(p, q);
	float dis = distance(p, q);

	vec3 op1 = p;
	vec3 op2 = p * coshf(dis / 2) + dir * sinhf(dis / 2);

	for (int i = 0; i != N + 1; i++) {
		vec3 pos = translateNode(translateNode(nodes[i].pos, op1), op2);
		if (pos.x == pos.x && pos.y == pos.y && pos.z == pos.z)
			nodes[i].pos = pos;
		nodes[i].velocity = vec3(0, 0, 0);
	}
}

void drawEdge(int i, int j) {
	vec3 poses[2];
	poses[0] = nodes[i].pos;
	poses[1] = nodes[j].pos;

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(poses), poses, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvcache), uvcache, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindTexture(GL_TEXTURE_2D, edgeTextureId);
	glDrawArrays(GL_LINES, 0, 2);
}

void drawCircle(int i) {
	static const float SINH = sinhf(RADIUS);
	static const float COSH = coshf(RADIUS);
	vec3 const& pos = nodes[i].pos;
	vec3 circle[RESOLUTION + 2];
	circle[0] = pos;
	for (int j = 0; j < RESOLUTION + 1; j++) {
		vec3 r;
		r.x = pos.x + coscache[j];
		r.y = pos.y + sincache[j];
		correctZ(r);
		vec3 v = direction(pos, r);
		circle[j + 1] = pos * COSH + v * SINH;
	}

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(circle), circle, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvcache), uvcache, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindTexture(GL_TEXTURE_2D, nodeTextureId[i]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, RESOLUTION + 2);
}


void onInitialization() {

	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(1.0f);
	gpuProgram.create(vertexSource, fragmentSource, "outColor");

	float MVPtransf[4][4] = { 1, 0, 0, 0,
							  0, 1, 0, 0,
							  0, 0, 1, 0,
							  0, 0, 0, 1 };
	int location = glGetUniformLocation(gpuProgram.getId(), "MVP");
	glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(2, vbo);

	for (int j = 0; j != TEXTURE_SIZE; j++)
		for (int k = 0; k != TEXTURE_SIZE; k++)
			edgeTexture[j * TEXTURE_SIZE + k] = vec4(1.0f, 1.0f, 0.0f, 1.0f);
	glGenTextures(1, &edgeTextureId);
	glBindTexture(GL_TEXTURE_2D, edgeTextureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGBA, GL_FLOAT, &edgeTexture);
	glGenTextures(N, nodeTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	for (int i = 0; i != N; i++) {
		vec4 primaryColor = vec4(float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, 1.0f);
		vec4 secondaryColor = vec4(float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, 1.0f);
		for (int j = 0; j != TEXTURE_SIZE / 2; j++)
			for (int k = 0; k != TEXTURE_SIZE; k++)
				nodeTexture[i][j * TEXTURE_SIZE + k] = primaryColor;
		for (int j = TEXTURE_SIZE / 2; j != TEXTURE_SIZE; j++)
			for (int k = 0; k != TEXTURE_SIZE; k++)
				nodeTexture[i][j * TEXTURE_SIZE + k] = secondaryColor;
		glBindTexture(GL_TEXTURE_2D, nodeTextureId[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGBA, GL_FLOAT, nodeTexture[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glUniform1i(glGetUniformLocation(gpuProgram.getId(), "samplerUnit"), 0);
	glActiveTexture(GL_TEXTURE0 + 0);

	const float INCREMENT = 2 * M_PI / RESOLUTION;
	float angle = 0.0f;
	for (int i = 0; i < RESOLUTION + 1; i++, angle += INCREMENT) {
		coscache[i] = cosf(angle);
		sincache[i] = sinf(angle);
		uvcache[i] = vec2((coscache[i] + 1) / 2, (sincache[i] + 1) / 2);
	}

	for (int i = 0; i != E; i++) {
		int a, b;
		do {
			a = rand() % N;
			b = rand() % N;
		} while (edges[a][b] || a == b);
		edges[a][b] = true;
		edges[b][a] = true;
	}

	for (int i = 0; i != N; i++) {
		vec3& pos = nodes[i].pos;
		pos.x = cosf(float(i) / N * 2 * M_PI) * START_SCALE;
		pos.y = sinf(float(i) / N * 2 * M_PI) * START_SCALE;
		correctZ(pos);
		nodes[i].velocity = vec3(0, 0, 0);
	}
	nodes[N].pos = vec3(0, 0, 1);

}

void onDisplay() {
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	for (int i = 0; i != N; i++)
		for (int j = 0; j < i; j++)
			if (edges[i][j])
				drawEdge(i, j);

	for (int i = 0; i != N; i++)
		drawCircle(i);

	glutSwapBuffers();
}

void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == ' ') {
		runs = true;
		heuristic();
		glutPostRedisplay();
	}
}

void onKeyboardUp(unsigned char key, int pX, int pY) {}

void onMouseMotion(int pX, int pY) {
	float cX = 2.0f * pX / windowWidth - 1;
	float cY = 1.0f - 2.0f * pY / windowHeight;

	if (length(vec2(cX, cY) - mouseBase) > 0.001) {
		translate(mouseBase, vec2(cX, cY));
		mouseBase = vec2(cX, cY);
		glutPostRedisplay();
	}
}

void onMouse(int button, int state, int pX, int pY) {
	float cX = 2.0f * pX / windowWidth - 1;
	float cY = 1.0f - 2.0f * pY / windowHeight;

	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		mouseBase = vec2(cX, cY);

}

void onIdle() {
	if (runs) {
		if (lastIterationTime == 0)
			lastIterationTime = glutGet(GLUT_ELAPSED_TIME);
		long time = glutGet(GLUT_ELAPSED_TIME);
		if (time - lastIterationTime > REFRESH_TIME) {
			while (time - lastIterationTime > REFRESH_TIME) {
				for (int i = 0; i != ITERATION_PER_FRAME; i++)
					iterate();
				lastIterationTime += REFRESH_TIME;
			}
			glutPostRedisplay();
		}
	}
}
