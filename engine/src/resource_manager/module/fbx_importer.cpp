﻿#include "fbx_importer.h"

using namespace ad_astris;
using namespace resource::impl;

bool FBXImporter::import(
	const std::string& path,
	std::vector<ModelCreateInfo>& outModelInfos,
	std::vector<TextureCreateInfo>& outTextureInfos,
	std::vector<MaterialCreateInfo>& materialInfos,
	const ecore::ModelConversionContext& conversionContext)
{
	return true;
}

