from db import db
from fastapi import APIRouter, BackgroundTasks
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from pymongo import ReturnDocument
from auto_lock import auto_lock_locker  # Import hàm dùng chung

router = APIRouter()

class ToggleRequest(BaseModel):
    lockerId: int
    action: str  # "lock" or "unlock"

@router.post("/api/toggle")
async def toggle_locker(data: ToggleRequest, background_tasks: BackgroundTasks):
    if data.action not in ["lock", "unlock"]:
        return JSONResponse(content={"error": "Invalid action"}, status_code=400)

    locker = await db.locker.find_one_and_update(
        {"number": data.lockerId},
        {"$set": {"isLocked": data.action == "lock"}},
        return_document=ReturnDocument.AFTER,
    )

    if not locker:
        return JSONResponse(content={"error": "Locker not found"}, status_code=404)
    # Nếu mở tủ thì tự động khóa lại sau 10 giây
    if data.action == "unlock":
        background_tasks.add_task(auto_lock_locker, locker["_id"], 10)

    locker["_id"] = str(locker["_id"])
    return {"locker": locker}