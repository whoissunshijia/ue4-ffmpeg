// Fill out your copyright notice in the Description page of Project Settings.


#include "EncodeData.h"

FEncodeData::FEncodeData():
	DataMemory(nullptr)
{
}

FEncodeData::~FEncodeData()
{
	if (DataMemory)
		FMemory::Free(DataMemory);
}

void FEncodeData::InitializeData(int size)
{
	DataMemory = (uint8*)FMemory::Realloc(DataMemory, size);
	datasize = size;
}

void FEncodeData::SetEncodeData(uint8* Src)
{	
	FMemory::StreamingMemcpy(DataMemory, Src, datasize);	
}

uint8* FEncodeData::GetData()
{
	return DataMemory != nullptr ? DataMemory : nullptr;
}

//////////////////////////////////////////////////////////////////

UCircleQueue::UCircleQueue():
	queue_ptr(nullptr)
{
	queue_head = 0;
	queue_tail = 0;
}

UCircleQueue::~UCircleQueue()
{
	delete[] queue_ptr;
}

void UCircleQueue::Init(int queue_len, int data_sized)
{
	queue_num = queue_len;
	queue_freenum = queue_num;
	queue_ptr = new FEncodeData[queue_num];
	for (int i = 0; i < queue_num; ++i)
	{
		queue_ptr[i].InitializeData(data_sized);
	}
}

bool UCircleQueue::InsertEncodeData(uint8* Src)
{
	if (IsFull())
		return false;
	queue_ptr[queue_tail].SetEncodeData(Src);
	--queue_freenum;
	queue_tail = (queue_tail + 1) % queue_num;
	return true;
}

bool UCircleQueue::PrcessEncodeData()
{
	if (IsEmpty())
		return false;
	encode_delegate.ExecuteIfBound(queue_ptr[queue_head].GetData());
	queue_head= (queue_head + 1) % queue_num;
	++queue_freenum;
	return true;
}

bool UCircleQueue::IsFull()
{
	return queue_freenum == 0 ? true : false;
}

bool UCircleQueue::IsEmpty()
{
	return queue_freenum == queue_num ? true : false;
}