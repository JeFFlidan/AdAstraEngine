#pragma once

#include "archetype_types.h"
#include "entity_types.h"
#include "core/tuple.h"
#include <vector>
#include <unordered_set>

namespace ad_astris::ecs
{
	class EntityManager;
	
	namespace constants
	{
		constexpr uint32_t ARCHETYPE_CHUNK_SIZE = 128 * 1024;
		constexpr uint32_t MAX_ENTITIES_IN_CNUNK = 1024;
	}

	class Subchunk
	{
		public:
			Subchunk() = default;
			Subchunk(uint8_t* startPtr, uint32_t subchunkSize, uint16_t structureSize)
				: _startPtr(startPtr), _subchunkSize(subchunkSize), _structureSize(structureSize) { }

			uint8_t* get_ptr() const
			{
				return _startPtr;
			}

			uint32_t get_subchunk_size() const
			{
				return _subchunkSize;
			}

			uint32_t get_structure_size() const
			{
				return _structureSize;
			}
		
		private:
			uint8_t* _startPtr{ nullptr };
			uint32_t _subchunkSize{ 0 };
			uint32_t _structureSize{ 0 };
	};
	
	struct ChunkStructure
	{
		std::vector<uint64_t> componentIds;
		std::unordered_map<uint64_t, uint32_t> sizeByComponentID;	// should be sorted
		std::vector<uint64_t> tagIDs;
		std::unordered_set<uint64_t> tagIDsSet;
		uint32_t numEntitiesPerChunk{ 0 };
		uint32_t sizeOfOneColumn{ 0 };
	};
	
	class ArchetypeChunk
	{
		public:
			ArchetypeChunk(uint32_t chunkSize, ChunkStructure& chunkStructure);
			~ArchetypeChunk();
		
			void add_several_instances(uint32_t count);
			void remove_several_instances(uint32_t count);

			void add_instance();
			void remove_instance();
		
			uint32_t get_elements_count();
			uint8_t* get_chunk();
		
			void set_component(uint32_t column, IComponent* component);
			Subchunk get_subchunk(uint64_t componentTypeId);
			uint8_t* get_entity_component(uint32_t column, uint64_t componentTypeId);

		private:
			uint8_t* _chunk{ nullptr };
			std::unordered_map<uint64_t, Subchunk> _componentIdToSubchunk;
			uint32_t _chunkSize{ 0 };
			uint32_t _elementsCount{ 0 };
	};

	class Archetype
	{
		friend EntityManager;
		
		public:
			Archetype(ArchetypeCreationContext& context);

			/**
			 * 
			 * @return entity row index in the archetype*/
			uint32_t add_entity(Entity& entity);
			void destroy_entity(Entity& entity, uint32_t rowIndex);

			void set_component(Entity& entity, uint32_t columnIndex, IComponent* tempComponent);
			void set_components(Entity& entity, uint32_t columnIndex, EntityCreationContext& creationContext);

			/**
			 * @warning THINK ABOUT TUPLES!!!!!
			 */
			template<typename ...TYPES>
			std::vector<Subchunk> get_subchunks()
			{
				std::vector<Subchunk> subchunks;
				for (auto& chunk : _chunks)
				{
					((subchunks.push_back(chunk.get_subchunk(TypeInfoTable::get_component_id<TYPES>()))), ...);
				}

				return subchunks;
			}

			template<typename T>
			std::vector<Subchunk> get_subchunks_of_one_type()
			{
				std::vector<Subchunk> subchunks;
				for (auto& chunk : _chunks)
				{
					subchunks.push_back(chunk.get_subchunk(TypeInfoTable::get_component_id<T>()));
				}

				return subchunks;
			}

			template<typename T>
			T* get_entity_component(Entity& entity, uint32_t columnIndex)
			{
				ArchetypeChunk& chunk = _chunks[_entityToChunk[entity]];
				return reinterpret_cast<T*>(chunk.get_entity_component(columnIndex, TypeInfoTable::get_component_id<T>()));
			}

			template<typename ...ARGS>
			Tuple<ARGS*...> get_entity_components(Entity& entity, uint32_t columnIndex)
			{
				ArchetypeChunk& chunk = _chunks[_entityToChunk[entity]];
				Tuple<ARGS*...> tuple{ get_converted_component<ARGS>(chunk, columnIndex)... };
				return tuple;
			}

			template<typename ...ARGS>
			std::vector<Tuple<ARGS...>> get_component_arrays()
			{
				// TODO
			}
		
			uint32_t get_chunk_size();
			uint32_t get_chunks_count();

			uint32_t get_entities_count_per_chunk(uint32_t chunkIndex);

			bool check_requirements_match(
				std::vector<uint64_t>& requiredComponentIDs,
				std::vector<uint64_t>& requiredTagIDs);

			template<typename T>
			bool has_component()
			{
				auto it = _chunkStructure.sizeByComponentID.find(TypeInfoTable::get_component_id<T>());
				return it == _chunkStructure.sizeByComponentID.end() ? false : true;
			}

			template<typename T>
			bool has_tag()
			{
				auto it = _chunkStructure.tagIDsSet.find(TypeInfoTable::get_tag_id<T>());
				return it == _chunkStructure.tagIDsSet.end() ? false : true;
			}

			FORCE_INLINE bool has_component(uint64_t componentID)
			{
				return _chunkStructure.sizeByComponentID.find(componentID) != _chunkStructure.sizeByComponentID.end();
			}

			FORCE_INLINE bool has_tag(uint64_t tagID)
			{
				return _chunkStructure.tagIDsSet.find(tagID) != _chunkStructure.tagIDsSet.end();
			}
		
			// I use this method for serialization
			void get_component_by_type_id(
				Entity& entity,
				uint32_t columnIndex,
				uint64_t typeId,
				uint8_t* tempComponentsArray);

			void* get_component_by_type_id(Entity& entity, uint32_t columnIndex, uint64_t typeID);
		
		private:
			std::unordered_map<Entity, uint16_t> _entityToChunk;
			std::vector<ArchetypeChunk> _chunks;
			std::vector<uint32_t> _freeColumns;

			ChunkStructure _chunkStructure;
		
			template<typename T>
			T* get_converted_component(ArchetypeChunk& chunk, uint64_t columnIndex)
			{
				return reinterpret_cast<T*>(chunk.get_entity_component(columnIndex, TypeInfoTable::get_component_id<T>()));
			}
	};
}