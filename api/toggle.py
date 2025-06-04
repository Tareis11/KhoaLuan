from fastapi import APIRouter
from fastapi.responses import JSONResponse
from motor.motor_asyncio import AsyncIOMotorClient
from pydantic import BaseModel
from pymongo import ReturnDocument  # 👈 THÊM dòng này
import os

router = APIRouter()
client = AsyncIOMotorClient(os.getenv("MONGODB_URI"))
db = client["locker_system"]


class ToggleRequest(BaseModel):
    lockerId: int
    action: str  # "lock" or "unlock"


@router.post("/api/toggle")
async def toggle_locker(data: ToggleRequest):
    if data.action not in ["lock", "unlock"]:
        return JSONResponse(content={"error": "Invalid action"}, status_code=400)

    locker = await db.locker.find_one_and_update(
        {"number": data.lockerId},
        {"$set": {"isLocked": data.action == "lock"}},
        return_document=ReturnDocument.AFTER,  # ✅ TRẢ VỀ SAU KHI UPDATE
    )

    if not locker:
        return JSONResponse(content={"error": "Locker not found"}, status_code=404)

    locker["_id"] = str(locker["_id"])
    return {"locker": locker}
