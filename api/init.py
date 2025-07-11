# 📄 locker-fastapi/api/init.py

from db import db
from fastapi import APIRouter
from fastapi.responses import JSONResponse
from pymongo.errors import PyMongoError

router = APIRouter()


@router.post("/api/init")
async def full_init():
    try:
        await db.locker.delete_many({})
        await db.temperature_log.delete_many({})

        locker_data = []
        for i in range(1, 4):
            locker = {
                "number": i,
                "isLocked": True,
                "times": 0,
                "code": "",
            }
            locker_data.append(locker)

        await db.locker.insert_many(locker_data)

        lockers = await db.locker.find().to_list(None)
        for locker in lockers:
            locker["_id"] = str(locker["_id"])
        return {"message": "Initialization successful", "lockers": lockers}
    except PyMongoError as e:
        print("Error initializing lockers:", e)
        return JSONResponse(
            content={"error": "Failed to initialize lockers"}, status_code=500
        )
