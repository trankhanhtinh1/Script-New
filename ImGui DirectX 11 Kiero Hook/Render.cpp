#include "Render.h"
#include "SDK/GameObject.h"
#include "SDK/AiManager.h"
#include <cmath>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Render
{
	// Global variables
	float viewMatrix[16];
	float projMatrix[16];
	float viewProjMatrix[16];

	int g_screenWidth = 1920;
	int g_screenHeight = 1080;

	size_t circlePoints = 64;
	std::vector<float> vSinTable;
	std::vector<float> vCosTable;

	// Internal struct for W2S
	struct Vector4 {
		float x, y, z, w;
	};

	void MultiplyMatrices(float* out, float* a, int row1, int col1, float* b, int row2, int col2)
	{
		int size = row1 * col2;
		for (int i = 0; i < row1; i++) {
			for (int j = 0; j < col2; j++) {
				float sum = 0.f;
				for (int k = 0; k < col1; k++)
					sum = sum + a[i * col1 + k] * b[k * col2 + j];
				out[i * col2 + j] = sum;
			}
		}
	}

	Vector2 WorldToScreen(const Vector3& pos)
	{
		Vector2 out = { 0.f, 0.f };
		Vector2 screen = { (float)g_screenWidth, (float)g_screenHeight };

		Vector4 clipCoords;
		clipCoords.x = pos.x * viewProjMatrix[0] + pos.y * viewProjMatrix[4] + pos.z * viewProjMatrix[8] + viewProjMatrix[12];
		clipCoords.y = pos.x * viewProjMatrix[1] + pos.y * viewProjMatrix[5] + pos.z * viewProjMatrix[9] + viewProjMatrix[13];
		clipCoords.z = pos.x * viewProjMatrix[2] + pos.y * viewProjMatrix[6] + pos.z * viewProjMatrix[10] + viewProjMatrix[14];
		clipCoords.w = pos.x * viewProjMatrix[3] + pos.y * viewProjMatrix[7] + pos.z * viewProjMatrix[11] + viewProjMatrix[15];

		if (clipCoords.w < 1.0f)
			clipCoords.w = 1.f;

		Vector2 M;
		M.x = clipCoords.x / clipCoords.w;
		M.y = clipCoords.y / clipCoords.w;

		out.x = (M.x + 1.0f) * (screen.x / 2.0f);
		out.y = (1.0f - M.y) * (screen.y / 2.0f);

		return out;
	}

	void InitCircle()
	{
		vCosTable.resize(circlePoints);
		vSinTable.resize(circlePoints);

		for (auto i = 0; i < circlePoints; i++)
		{
			vCosTable[i] = cos(static_cast<float>(i) * (M_PI * 2.f) / static_cast<float>(circlePoints - 1));
			vSinTable[i] = sin(static_cast<float>(i) * (M_PI * 2.f) / static_cast<float>(circlePoints - 1));
		}
	}

	void DrawCircle3D(ImDrawList* draw, Vector3 vPos, float flPoints, float flRadius, ImU32 clrColor, float flThickness)
	{
		float flPoint = 3.14159265359f * 2.0f / flPoints;

		for (float flAngle = 0; flAngle < (3.14159265359f * 2.0f); flAngle += flPoint)
		{
			Vector3 vStart(flRadius * cosf(flAngle) + vPos.x, vPos.y, flRadius * sinf(flAngle) + vPos.z);
			Vector3 vEnd(flRadius * cosf(flAngle + flPoint) + vPos.x, vPos.y, flRadius * sinf(flAngle + flPoint) + vPos.z);

			ImVec2 startPos = ImVec2(WorldToScreen(vStart).x, WorldToScreen(vStart).y);
			ImVec2 endPos = ImVec2(WorldToScreen(vEnd).x, WorldToScreen(vEnd).y);

			draw->AddLine(startPos, endPos, clrColor, flThickness);
		}
	}

	void DrawCirclee(ImDrawList* canvas, const Vector3& worldPos, float radius, bool filled, int numPoints, ImColor color, float thickness)
	{
		if (numPoints >= 200) return;

		static ImVec2 points[200];
		float step = 6.2831f / numPoints;

		for (int i = 0; i < numPoints; i++) {
			Vector3 worldPoint = {
				worldPos.x + radius * cos(i * step),
				worldPos.y,
				worldPos.z + radius * sin(i * step)
			};

			Vector2 screenPoint = WorldToScreen(worldPoint);
			points[i] = ImVec2(screenPoint.x, screenPoint.y);
		}

		if (filled)
			canvas->AddConvexPolyFilled(points, numPoints, color);
		else
			canvas->AddPolyline(points, numPoints, color, true, thickness);
	}

	void DrawLine(ImDrawList* drawList, float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a)
	{
		ImU32 color = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
		drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, thickness);
	}

	void DrawCircleAt(ImDrawList* draw, const Vector3 worldPos, float radius, bool filled, int numPoints, float thickness, ImColor Col)
	{
		auto list = draw;
		Vector3 worldSpace = worldPos;
		for (auto i = 0; i < circlePoints; i++)
		{
			worldSpace.x = worldPos.x + vCosTable[i] * radius;
			worldSpace.z = worldPos.z + vSinTable[i] * radius;
			auto e = WorldToScreen(worldSpace);
			list->PathLineTo(ImVec2(e.x, e.y));
		}
		list->PathStroke(Col, false, thickness);
	}
	
	void DrawRectangle3D(ImDrawList* draw, const Vector3& startPos, const Vector3& endPos, float width, ImU32 fillColor, ImU32 borderColor, float thickness)
	{
		if (!draw) return;
		
		// Calculate direction vector
		Vector3 direction = endPos - startPos;
		float length = sqrtf(direction.x * direction.x + direction.z * direction.z);
		if (length < 1.0f) return;
		
		// Normalize direction
		direction.x /= length;
		direction.z /= length;
		
		// Calculate perpendicular (for width)
		Vector3 perpendicular;
		perpendicular.x = -direction.z;
		perpendicular.y = 0;
		perpendicular.z = direction.x;
		
		// Calculate 4 corners of the rectangle
		float halfWidth = width / 2.0f;
		Vector3 corners[4];
		corners[0] = Vector3(startPos.x + perpendicular.x * halfWidth, startPos.y, startPos.z + perpendicular.z * halfWidth);
		corners[1] = Vector3(startPos.x - perpendicular.x * halfWidth, startPos.y, startPos.z - perpendicular.z * halfWidth);
		corners[2] = Vector3(endPos.x - perpendicular.x * halfWidth, endPos.y, endPos.z - perpendicular.z * halfWidth);
		corners[3] = Vector3(endPos.x + perpendicular.x * halfWidth, endPos.y, endPos.z + perpendicular.z * halfWidth);
		
		// Convert to screen coordinates
		ImVec2 screenCorners[4];
		bool allVisible = true;
		for (int i = 0; i < 4; i++) {
			Vector2 screen = WorldToScreen(corners[i]);
			if (screen.x < -100 || screen.y < -100 || screen.x > g_screenWidth + 100 || screen.y > g_screenHeight + 100) {
				allVisible = false;
			}
			screenCorners[i] = ImVec2(screen.x, screen.y);
		}
		
		// Draw filled rectangle
		if ((fillColor & IM_COL32_A_MASK) != 0) {
			draw->AddQuadFilled(screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], fillColor);
		}
		
		// Draw border
		if ((borderColor & IM_COL32_A_MASK) != 0 && thickness > 0) {
			draw->AddQuad(screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], borderColor, thickness);
		}
	}
	
	void DrawAiManagerDebug(ImDrawList* draw, SDK::GameObject* obj)
	{
		if (!obj || !obj->IsValid()) return;
		
		SDK::AiManager aiManager = obj->GetAiManager();
		if (!aiManager.IsValid()) return;
		
		Vector3 objPos = obj->GetPosition();
		Vector2 screenPos = WorldToScreen(objPos);
		
		if (screenPos.x < 0 || screenPos.y < 0 || screenPos.x > g_screenWidth || screenPos.y > g_screenHeight) {
			return; // Off screen
		}
		
		// Get AiManager data
		bool isMoving = aiManager.IsMoving();
		bool isDashing = aiManager.IsDashing();
		float dashSpeed = aiManager.GetDashSpeed();
		float moveSpeed = aiManager.GetMoveSpeed();
		Vector3 startPath = aiManager.GetStartPath();
		Vector3 endPath = aiManager.GetEndPath();
		Vector3 moveVec = aiManager.GetMoveVec3();
		Vector3 serverPos = aiManager.GetServerPosition();
		int currentSegment = aiManager.GetCurrentSegment();
		int segmentsCount = aiManager.GetSegmentsCount();
		
		// Draw text info (top-left corner)
		float textX = 10.0f;
		float textY = 10.0f;
		float lineHeight = 20.0f;
		
		// Status
		std::string status = "IDLE";
		ImU32 statusColor = IM_COL32(128, 128, 128, 255); // Gray
		if (isDashing) {
			status = "DASHING";
			statusColor = IM_COL32(255, 0, 255, 255); // Magenta
		} else if (isMoving) {
			status = "MOVING";
			statusColor = IM_COL32(0, 255, 0, 255); // Green
		}
		
		draw->AddText(ImVec2(textX, textY), statusColor, ("Status: " + status).c_str());
		textY += lineHeight;
		
		// Speed info
		std::ostringstream speedStr;
		speedStr << std::fixed << std::setprecision(1);
		speedStr << "Speed: " << moveSpeed << " units/sec";
		if (isDashing && dashSpeed > 0) {
			speedStr << " (Dash: " << dashSpeed << ")";
		}
		draw->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), speedStr.str().c_str());
		textY += lineHeight;
		
		// Path segments
		std::ostringstream segmentStr;
		segmentStr << "Segment: " << currentSegment << "/" << segmentsCount;
		draw->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), segmentStr.str().c_str());
		textY += lineHeight;
		
		// Move direction
		if (moveVec.Length() > 0.1f) {
			std::ostringstream dirStr;
			dirStr << std::fixed << std::setprecision(2);
			dirStr << "Direction: (" << moveVec.x << ", " << moveVec.y << ", " << moveVec.z << ")";
			draw->AddText(ImVec2(textX, textY), IM_COL32(200, 200, 255, 255), dirStr.str().c_str());
			textY += lineHeight;
		}
		
		// Draw path visualization using actual waypoints (handles pathfinding around walls)
		std::vector<Vector3> waypoints = aiManager.GetWaypoints();
		
		if (waypoints.size() > 0) {
			ImU32 lineColor = isDashing ? IM_COL32(255, 0, 255, 200) : IM_COL32(0, 255, 0, 200);
			
			// Draw waypoints and connect them
			Vector2 lastScreen = {0, 0};
			for (size_t i = 0; i < waypoints.size(); i++) {
				Vector2 waypointScreen = WorldToScreen(waypoints[i]);
				
				if (waypointScreen.x > 0 && waypointScreen.y > 0) {
					// Draw waypoint marker
					bool isStart = (i == 0);
					bool isEnd = (i == waypoints.size() - 1);
					
					if (isStart) {
						// Start point (green)
						draw->AddCircleFilled(ImVec2(waypointScreen.x, waypointScreen.y), 5.0f, IM_COL32(0, 255, 0, 255));
						draw->AddText(ImVec2(waypointScreen.x + 10, waypointScreen.y - 10), IM_COL32(0, 255, 0, 255), "Start");
					} else if (isEnd) {
						// End point (red)
						draw->AddCircleFilled(ImVec2(waypointScreen.x, waypointScreen.y), 5.0f, IM_COL32(255, 0, 0, 255));
						draw->AddText(ImVec2(waypointScreen.x + 10, waypointScreen.y - 10), IM_COL32(255, 0, 0, 255), "End");
					} else {
						// Intermediate waypoint (yellow, smaller)
						draw->AddCircleFilled(ImVec2(waypointScreen.x, waypointScreen.y), 3.0f, IM_COL32(255, 255, 0, 200));
					}
					
					// Draw line to previous waypoint
					if (lastScreen.x > 0 && lastScreen.y > 0) {
						draw->AddLine(ImVec2(lastScreen.x, lastScreen.y), 
						             ImVec2(waypointScreen.x, waypointScreen.y), 
						             lineColor, 2.0f);
					}
					
					lastScreen = waypointScreen;
				}
			}
		} else if (startPath.x > 0 && endPath.x > 0) {
			// Fallback: Draw straight line if no waypoints
			Vector2 startScreen = WorldToScreen(startPath);
			Vector2 endScreen = WorldToScreen(endPath);
			
			if (startScreen.x > 0 && startScreen.y > 0) {
				draw->AddCircleFilled(ImVec2(startScreen.x, startScreen.y), 5.0f, IM_COL32(0, 255, 0, 255));
				draw->AddText(ImVec2(startScreen.x + 10, startScreen.y - 10), IM_COL32(0, 255, 0, 255), "Start");
			}
			
			if (endScreen.x > 0 && endScreen.y > 0) {
				draw->AddCircleFilled(ImVec2(endScreen.x, endScreen.y), 5.0f, IM_COL32(255, 0, 0, 255));
				draw->AddText(ImVec2(endScreen.x + 10, endScreen.y - 10), IM_COL32(255, 0, 0, 255), "End");
				
				if (startScreen.x > 0 && startScreen.y > 0) {
					ImU32 lineColor = isDashing ? IM_COL32(255, 0, 255, 200) : IM_COL32(0, 255, 0, 200);
					draw->AddLine(ImVec2(startScreen.x, startScreen.y), ImVec2(endScreen.x, endScreen.y), lineColor, 2.0f);
				}
			}
		}
		
		// Draw movement direction arrow from current position
		if (isMoving && moveVec.Length() > 0.1f) {
			Vector3 arrowEnd = objPos + (moveVec * 200.0f); // 200 units in direction
			Vector2 arrowEndScreen = WorldToScreen(arrowEnd);
			
			if (arrowEndScreen.x > 0 && arrowEndScreen.y > 0) {
				ImU32 arrowColor = isDashing ? IM_COL32(255, 0, 255, 255) : IM_COL32(0, 255, 255, 255);
				draw->AddLine(ImVec2(screenPos.x, screenPos.y), ImVec2(arrowEndScreen.x, arrowEndScreen.y), arrowColor, 3.0f);
				
				// Draw arrow head (simple triangle)
				float arrowSize = 15.0f;
				Vector2 dir = Vector2(arrowEndScreen.x - screenPos.x, arrowEndScreen.y - screenPos.y);
				float dirLen = sqrtf(dir.x * dir.x + dir.y * dir.y);
				if (dirLen > 0.1f) {
					dir.x /= dirLen;
					dir.y /= dirLen;
					
					Vector2 perp = Vector2(-dir.y, dir.x);
					Vector2 arrowTip = arrowEndScreen;
					Vector2 arrowLeft = Vector2(arrowEndScreen.x - dir.x * arrowSize + perp.x * arrowSize * 0.5f,
					                            arrowEndScreen.y - dir.y * arrowSize + perp.y * arrowSize * 0.5f);
					Vector2 arrowRight = Vector2(arrowEndScreen.x - dir.x * arrowSize - perp.x * arrowSize * 0.5f,
					                             arrowEndScreen.y - dir.y * arrowSize - perp.y * arrowSize * 0.5f);
					
					draw->AddTriangleFilled(
						ImVec2(arrowTip.x, arrowTip.y),
						ImVec2(arrowLeft.x, arrowLeft.y),
						ImVec2(arrowRight.x, arrowRight.y),
						arrowColor
					);
				}
			}
		}
		
		// Draw server position (yellow circle) if different from object position
		if (serverPos.x > 0) {
			float serverDist = objPos.Distance(serverPos);
			if (serverDist > 5.0f) { // Only draw if significantly different
				Vector2 serverScreen = WorldToScreen(serverPos);
				if (serverScreen.x > 0 && serverScreen.y > 0) {
					draw->AddCircle(ImVec2(serverScreen.x, serverScreen.y), 8.0f, IM_COL32(255, 255, 0, 255), 16, 2.0f);
					draw->AddText(ImVec2(serverScreen.x + 10, serverScreen.y - 10), IM_COL32(255, 255, 0, 255), "Server");
				}
			}
		}
	}
}
