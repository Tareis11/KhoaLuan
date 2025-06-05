from db import db
from fastapi import APIRouter
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from datetime import datetime
import random

router = APIRouter()


# POST /api/request (ESP gửi mã code)
class RequestCode(BaseModel):
    code: str


# @router.post("/api/request")
# async def request_from_esp(data: RequestCode):
#     locker = await db.locker.find_one({"code": data.code})
#     if locker:
#         return {"found": True, "lockerNumber": locker["number"]}
#     else:
#         return {"found": False}


# ✅ GET /api/request (web gọi để tạo code mới)
@router.get("/api/request")
async def generate_code():
    def generate_unique_code():
        timestamp = str(int(datetime.utcnow().timestamp() * 1000))
        random_part = str(random.randint(0, 999)).zfill(3)
        return (timestamp + random_part)[-9:]

    locker = await db.locker.find_one({"code": ""})
    if not locker:
        return JSONResponse(content={"error": "No available lockers"}, status_code=404)

    # Generate unique code
    while True:
        code = generate_unique_code()
        if not await db.locker.find_one({"code": code}):
            break

    await db.locker.update_one(
        {"_id": locker["_id"]}, {"$set": {"code": code, "isLocked": True}}
    )

    updated = await db.locker.find_one({"_id": locker["_id"]})
    updated["_id"] = str(updated["_id"])
    return {"locker": updated}
