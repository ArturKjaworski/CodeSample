#include "pch.h"
#include "Game.h"

#include <iostream>

namespace
{
	const glm::vec3 normals[4] =
	{
		{  0, 0,  1 },
		{ -1, 0,  0 },
		{  0, 0, -1 },
		{  1, 0,  0 }
	};

	const int BLOCK_COUNT_X = 7;
	const int BLOCK_COUNT_Z = 5;
}

Game::Game()
{
}

Game::~Game()
{
	for (auto& obj : gObjects)
		if (obj != nullptr)
		{
			delete(obj);
			obj = nullptr;
		}
		
	destroyedObjsID.clear();
	gObjects.clear();

	if (Network::GetInstance().type == Network::EConnectionType::Client)
		Network::GetInstance().sendToServer("quit");
	else
		Network::GetInstance().sendToAll("quit");
}

Game & Game::GetInstance()
{
	static Game game;
	return game;
}

void Game::Init()
{
	gObjects.push_back(new GameObject("res/models/bg1.obj", "res/tex/bricks.jpg", GameObject::EObjectType::BG, glm::vec3(0.0, 0.0, 0.0)));

	enemy = new Player(nullptr, "res/models/platform.obj", "res/tex/bricks.jpg", GameObject::EObjectType::PLAYER, glm::vec3(0.0, 0.0, -20.0));
	gObjects.push_back(enemy);

	int id = 0;
	for (int ii = 0; ii < BLOCK_COUNT_X; ++ii)
		for (int jj = 0; jj < BLOCK_COUNT_Z; ++jj)
		{
			glm::vec3 pos((-1.75 * (BLOCK_COUNT_X - 1)) + ii * 3.5f, 0, 9.f - jj * 4.f);
			Block* block = new Block(id, "res/models/block.obj", "res/tex/bricks2.jpg", GameObject::EObjectType::BLOCK, pos);
			gObjects.push_back(block);
			++id;

			if (minBoundary.x > pos.x)
				minBoundary.x = pos.x - (block->GetHitBoxSize().x / 2);
			if (maxBoundary.x < pos.x)
				maxBoundary.x = pos.x + (block->GetHitBoxSize().x / 2);

			if (minBoundary.z > pos.z)
				minBoundary.z = pos.z - (block->GetHitBoxSize().z / 2);
			if (maxBoundary.z < pos.z)
				maxBoundary.z = pos.z + (block->GetHitBoxSize().z / 2);
		}

	Ball* ball = new Ball("res/models/ball.obj", "res/tex/bricks.jpg", GameObject::EObjectType::BALL, glm::vec3(0.0, 0.0, 18.5));
	gObjects.push_back(ball);

	Ball* enemyBall = new Ball("res/models/ball.obj", "res/tex/bricks.jpg", GameObject::EObjectType::BALL, glm::vec3(0.0, 0.0, -18.5));
	gObjects.push_back(enemyBall);	//enemy ball
	enemy->SetBall(enemyBall);

	player = new Player(ball, "res/models/platform.obj", "res/tex/bricks.jpg", GameObject::EObjectType::PLAYER, glm::vec3(0.0, 0.0, 20.0));
	gObjects.push_back(player); //our player

	isRunning = true;
}

void Game::Update(float deltaTime)
{
	if (!isRunning)
	{
		return;
	}

	Network& network = Network::GetInstance();
	if (network.type == Network::EConnectionType::Server)
	{
		std::string result = network.recv();
	
		std::string msg("");

		int blockCount = 0;

		for (GameObject* obj : gObjects)
			if (obj != nullptr && obj->type != GameObject::EObjectType::BG)
			{
				if ((result != "") && (obj->type == GameObject::EObjectType::PLAYER) && obj != player)
				{
					switch (result[0])
					{
						case 'a':		//left
						{
							enemy->MoveRight(0.2f);
							break;
						}

						case 'd':		//right
						{
							enemy->MoveRight(-0.2f);
							break;
						}

						case 'f':		//fire
						{
							enemy->Shoot();
							break;
						}
					}
				}

				obj->Update(deltaTime);

				if (obj->type != GameObject::EObjectType::BLOCK)
				{
					glm::vec3 enemyPos = obj->GetTransform().GetPos();
					//FORMAT   pos.x,pos.z;(...);did,id,id;		||		d = shows where list of ids starts id = blockid which are destroyed
					msg += std::to_string(-enemyPos.x) + ',' + std::to_string(-enemyPos.z) + ';';
				}

				if (obj->type == GameObject::EObjectType::BLOCK)
					++blockCount;
			}

		msg += "d";
		for (int ii = 0; ii < destroyedObjsID.size(); ++ii)
			msg += std::to_string(destroyedObjsID[ii]) + ',';

		if (blockCount == 0)
		{
			//game is finished - check points
			isRunning = false;
			system("cls");
			std::cout << (enemy->GetScore() <= player->GetScore() ? "\n\tYou win\n" : "\n\tYou lose\n");
			std::cout << "\nYour score: " << player->GetScore() << std::endl;
			std::cout << "\nPlayer 2 score: " << enemy->GetScore() << std::endl;

			std::string msg = "s" + std::to_string(enemy->GetScore()) + "," + std::to_string(player->GetScore()) + ",";
			network.sendToAll(msg);


			player->Reset();
			enemy->Reset();
		}
		sendInterval += deltaTime;

		//	if (sendInterval >= 0.015f)		//need prediction and interpolation on client side
		{
			network.sendToAll(msg);
			sendInterval = 0.0f;
		}
	}
	else
	{
		std::string res = network.recv();
		if (res[0] != 's')
			UpdateState(res);
		else
		{

			std::string tmp = "";
			int playerScore = -1000;
			int enemyScore = -1000;

			for (char c : res)
			{
				if (c == 's')
					continue;

				if (c == ',')
				{
					if (playerScore == -1000)
						playerScore = std::atof(tmp.c_str());
					else
						enemyScore = std::atof(tmp.c_str());

					tmp = "";
					continue;
				}
				tmp += c;
			}

			isRunning = false;
			system("cls");
			std::cout << (enemyScore <= playerScore ? "\n\tYou win\n" : "\n\tYou lose\n");
			std::cout << "\nYour score: " << playerScore << std::endl;
			std::cout << "\nPlayer 2 score: " << enemyScore << std::endl;
		}
	}
}

//FORMAT    pos.x,pos.z;(...);did,id,id;		||		 id = blockid which are destroyed
void Game::UpdateState(const std::string& state)
{
	if (!isRunning)
		return;

std::pair<std::vector<glm::vec3>, std::set<int>> pairResult = ParseMsg(state);

	if (pairResult.first.empty())
		return;
	
	for (int ii = gObjects.size() - 1; ii >= 0; --ii)
	{
		if (gObjects[ii]->type == GameObject::EObjectType::BG)
			continue;

		if (gObjects[ii]->type == GameObject::EObjectType::BLOCK)
		{
			auto res = pairResult.second.find(dynamic_cast<Block*>(gObjects[ii])->GetID());
			if (res != pairResult.second.end())
			{
				gObjects.erase(gObjects.begin() + ii);
				pairResult.second.erase(res);
			}
				
			continue;
		}

		gObjects[ii]->GetTransform().SetPos(pairResult.first.front());
		pairResult.first.erase(pairResult.first.begin() + 0);
	}
}

void Game::SetGameToRunning(bool running)
{
	isRunning = running; 
}

std::pair<std::vector<glm::vec3>, std::set<int>> Game::ParseMsg(const std::string& state)
{
	std::string tmp = "";
	std::vector<glm::vec3> newPos;
	float posX = 0.f;
	std::set<int> deletedBlocks;

	if (state == "")
		return std::make_pair(newPos, deletedBlocks);

	bool des = false;
	for (char c : state)
	{
		if (!des)
		{
			if (c == ',')
			{
				posX = std::atof(tmp.c_str());
				tmp = "";
				continue;
			}

			if (c == ';')
			{
				newPos.push_back(glm::vec3(posX, 0, std::atof(tmp.c_str())));
				posX = 0.f;
				tmp = "";
				continue;
			}

			if (c == 'd')
			{
				des = true;
				continue;
			}
		}
		else if (c == ',')
		{
			int id = std::atoi(tmp.c_str());

			deletedBlocks.insert(BLOCK_COUNT_X * BLOCK_COUNT_Z - 1 - std::atoi(tmp.c_str()));

			tmp = "";
			continue;
		}

		tmp += c;
	}

	return std::make_pair(newPos, deletedBlocks);
}

std::pair<glm::vec3, glm::vec3> Game::CheckCollision(const glm::vec3& oldPos, const glm::vec3 & newPos, const glm::vec3& forwardVec)
{
	//check edges
	glm::vec3 ballPos = newPos;
	glm::vec3 newForward = forwardVec;

#pragma region edges
	if (ballPos.x <= -14)
	{
		float n = (ballPos.x - (-14)) / forwardVec.x;

		ballPos.z -= (forwardVec.z * n);
		ballPos.x = -14;

		newForward = glm::normalize(glm::reflect(forwardVec, normals[3]));

		return std::make_pair(ballPos, newForward);
	}

	if (ballPos.x >= 14)
	{
		float n = (ballPos.x - 14) / forwardVec.x;

		ballPos.z -= (forwardVec.z * n);
		ballPos.x = 14;

		newForward = glm::normalize(glm::reflect(forwardVec, normals[1]));
		return std::make_pair(ballPos, newForward);
	}

	if (ballPos.z <= -21.5 || ballPos.z >= 21.5)
	{
		newForward = glm::vec3(0, 0, 0); 

		if (oldPos == player->GetBall()->GetTransform().GetPos())
		{
			glm::vec3 pos = player->GetTransform().GetPos();
			ballPos = glm::vec3(pos.x, 0, pos.z - Ball::radius/2);
			player->AddToScore(-30);
		}			
		else
		{
			glm::vec3 pos = enemy->GetTransform().GetPos();
			ballPos = glm::vec3(pos.x, 0, pos.z + Ball::radius / 2);
					
			enemy->AddToScore(-30);
		}
		
		return std::make_pair(ballPos, newForward);
	}

#pragma endregion

	for (size_t ii = 0; ii < gObjects.size(); ++ii)
	{
		//maxBoundary, min Boundary = rectangle where blocks exists
#pragma region platforms
		if ((ballPos.z - Ball::radius) > maxBoundary.z || (ballPos.z + Ball::radius) < minBoundary.z)
		{
			if (gObjects[ii]->type == GameObject::EObjectType::PLAYER)
				if (IsCollide(ballPos, gObjects[ii]))
				{
					glm::vec3 playerBoxSize = gObjects[ii]->GetHitBoxSize() / 2.f;
					glm::vec3 playerPos = gObjects[ii]->GetTransform().GetPos();

					if ((ballPos.x + Ball::radius >= playerPos.x - playerBoxSize.x) &&
						(ballPos.x - Ball::radius <= playerPos.x + playerBoxSize.x))
					{
						if (ballPos.z > 0 && ballPos.z + Ball::radius >= playerPos.z - playerBoxSize.z)
						{
							float diff = (ballPos.z) - (playerPos.z - playerBoxSize.z);			//how deep in platform
							float n = diff / forwardVec.z;
							ballPos -= n * forwardVec;
						}

						if (ballPos.z < 0 && ballPos.z - Ball::radius <= playerPos.z - playerBoxSize.z)
						{
							float diff = (ballPos.z) - (playerPos.z - playerBoxSize.z);			//how deep in platform
							float n = diff / forwardVec.z;
							ballPos -= n * forwardVec;
						}

						newForward = glm::normalize(glm::reflect(forwardVec, normals[ballPos.z > 0 ? 2 : 0]));
						return std::make_pair(ballPos, newForward);
					}
				}
		}
#pragma endregion
		
#pragma region ball
		if (gObjects[ii]->type == GameObject::EObjectType::BALL && oldPos != gObjects[ii]->GetTransform().GetPos())
		{
			auto dist = glm::distance(gObjects[ii]->GetTransform().GetPos(), oldPos);
			if (glm::distance(gObjects[ii]->GetTransform().GetPos(), oldPos) <= 2 * Ball::radius)
			{
				glm::vec3 normal = glm::normalize(ballPos - gObjects[ii]->GetTransform().GetPos());

				float diff = glm::distance(gObjects[ii]->GetTransform().GetPos(), ballPos) - 2 * Ball::radius;			//how deep in platform

				float n = diff / forwardVec.z;

				ballPos -= n * forwardVec;

				newForward = glm::normalize(glm::reflect(forwardVec, normal));
				return std::make_pair(ballPos, newForward);
			}
		}
#pragma endregion

#pragma region blocks
		if (gObjects[ii]->type == GameObject::EObjectType::BLOCK)
		{
			if (IsCollide(ballPos, gObjects[ii]))
			{
				if (oldPos == player->GetBall()->GetTransform().GetPos())
					player->AddToScore(20);
				else
					enemy->AddToScore(20);

				glm::vec3 objBoxSize = gObjects[ii]->GetHitBoxSize() / 2.f;
				glm::vec3 objPos = gObjects[ii]->GetTransform().GetPos();

				float n = (ballPos.z - Ball::radius - (objPos.z + objBoxSize.z)) / forwardVec.z;
				glm::vec3 tmpPos = ballPos - forwardVec * n;

				//IF's below can be shortened (DRY failed)

				//case for collisions on block edge
				if (((tmpPos.x - Ball::radius/2 > objPos.x + objBoxSize.x) || (tmpPos.x + Ball::radius/2 < objPos.x - objBoxSize.x)) && ((tmpPos.z - Ball::radius/2 > objPos.z + objBoxSize.z) || (tmpPos.z + Ball::radius/2 < objPos.z - objBoxSize.z)))
				{
					gObjects[ii]->OnHit();
					destroyedObjsID.push_back(dynamic_cast<Block*>(gObjects[ii])->GetID());

					gObjects.erase(gObjects.begin() + ii);
					return std::make_pair(ballPos, glm::normalize(-forwardVec));
				}

				if ((tmpPos.x >= objPos.x - objBoxSize.x) && (tmpPos.x <= objPos.x + objBoxSize.x))
				{
					newForward = glm::normalize(glm::reflect(forwardVec, normals[0]));

					gObjects[ii]->OnHit();
					destroyedObjsID.push_back(dynamic_cast<Block*>(gObjects[ii])->GetID());

					gObjects.erase(gObjects.begin() + ii);

					return std::make_pair(ballPos, newForward);
				}

				n = (ballPos.z - (objPos.z - objBoxSize.z)) / forwardVec.z;
				tmpPos = ballPos - forwardVec * n;

				if ((tmpPos.x > objPos.x - objBoxSize.x) && (tmpPos.x < objPos.x + objBoxSize.x))
				{
					newForward = glm::normalize(glm::reflect(forwardVec, normals[2]));

					gObjects[ii]->OnHit();
					destroyedObjsID.push_back(dynamic_cast<Block*>(gObjects[ii])->GetID());
					gObjects.erase(gObjects.begin() + ii);

					return std::make_pair(ballPos, newForward);
				}

				n = (ballPos.x - (objPos.x - objBoxSize.x)) / forwardVec.x;
				tmpPos = ballPos - forwardVec * n;

				if ((tmpPos.z >= objPos.z - objBoxSize.z) && (tmpPos.z <= objPos.z + objBoxSize.z))
				{
					newForward = glm::normalize(glm::reflect(forwardVec, normals[1]));

					gObjects[ii]->OnHit();
					destroyedObjsID.push_back(dynamic_cast<Block*>(gObjects[ii])->GetID());
					gObjects.erase(gObjects.begin() + ii);

					return std::make_pair(ballPos, newForward);
				}

				n = (ballPos.x - (objPos.x + objBoxSize.x)) / forwardVec.x;
				tmpPos = ballPos - forwardVec * n;

				if ((tmpPos.z > objPos.z - objBoxSize.z) && (tmpPos.z < objPos.z + objBoxSize.z))
				{
					newForward = glm::normalize(glm::reflect(forwardVec, normals[3]));

					gObjects[ii]->OnHit();
					destroyedObjsID.push_back(dynamic_cast<Block*>(gObjects[ii])->GetID());
					gObjects.erase(gObjects.begin() + ii);

					return std::make_pair(ballPos, newForward);
				}
			}
		}
#pragma endregion
	}
	return std::make_pair(ballPos, newForward);
}

bool Game::IsCollide(const glm::vec3& ballPos, GameObject* gObj)
{
	glm::vec3 objPos = gObj->GetTransform().GetPos();

	auto tmpPos = objPos;

	bool noresult = false;

	tmpPos.x -= gObj->GetHitBoxSize().x / 2;
	tmpPos.z += gObj->GetHitBoxSize().z / 2;

	glm::vec3 ballVec = ballPos - tmpPos;

	float result = glm::dot(normals[0], ballVec);

	noresult |= (result >= 0 ? true : false);

//left top

	tmpPos = objPos;
	tmpPos.x -= gObj->GetHitBoxSize().x / 2;
	tmpPos.z -= gObj->GetHitBoxSize().z / 2;

	ballVec = ballPos - tmpPos;

	result = glm::dot(normals[1], ballVec);
	noresult |= (result >= 0 ? true : false);


//right top

	tmpPos = objPos;
	tmpPos.x += gObj->GetHitBoxSize().x / 2;
	tmpPos.z -= gObj->GetHitBoxSize().z / 2;

	ballVec = ballPos - tmpPos;

	result = glm::dot(normals[2], ballVec);

	noresult |= (result >= 0 ? true : false);

//right bottom

	tmpPos = objPos;
	tmpPos.x += gObj->GetHitBoxSize().x / 2;
	tmpPos.z += gObj->GetHitBoxSize().z / 2;

	ballVec = ballPos - tmpPos;

	result = glm::dot(normals[3], ballVec);

	noresult |= (result >= 0 ? true : false);

	return !noresult;
}
