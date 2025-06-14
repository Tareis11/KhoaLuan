from db import db
from fastapi import APIRouter, BackgroundTasks
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from datetime import datetime, timedelta
from auto_lock import auto_lock_locker

router = APIRouter()


class ResetRequest(BaseModel):
    code: str


@router.post("/api/reset")
async def reset_locker(data: ResetRequest, background_tasks: BackgroundTasks):
    now_vn = datetime.utcnow() + timedelta(hours=7)
    # Tìm locker theo mã code
    locker = await db.locker.find_one({"code": data.code})
    if not locker:
        return JSONResponse(content={"error": "Locker not found"}, status_code=404)

    # Cập nhật trạng thái
    await db.locker.update_one(
        {"_id": locker["_id"]}, {"$set": {"isLocked": False, "times": 0, "code": ""}}
    )
    background_tasks.add_task(auto_lock_locker, locker["_id"])

    # Ghi lịch sử reset
    await db.locker_history.insert_one(
        {
            "lockerId": str(locker["_id"]),
            "lockerNumber": locker["number"],
            "action": "reset",
            "code": locker["code"],
            "timestamp": now_vn,
        }
    )

    updated = await db.locker.find_one({"_id": locker["_id"]})
    updated["_id"] = str(updated["_id"])
    return {"message": "Locker reset", "locker": updated}
