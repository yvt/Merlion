#pragma once

namespace mcore
{
	template <class TKey>
	class Balancer
	{
	public:
		struct Item
		{
			TKey key;
			double current;
			double desired;
		};
		
	private:
		struct InternalItem
		{
			const Item& item;
			double rank;
			
			InternalItem(const Item& item):
			item(item),
			rank(0.0) { }
		};
		
	public:
		
		template <class TCollection>
		boost::optional<TKey> performBalancing(const TCollection& col)
		{
			std::vector<InternalItem> items;
			double totalDesired = 0.0;
			double totalCurrent = 1.e-10;
			
			items.reserve(col.size());
			for (const Item& item: col) {
				if (item.desired <= 0.0) {
					continue;
				}
				items.push_back(item);
				totalCurrent += item.current;
				totalDesired += item.desired;
			}
			
			if (items.empty())
				return boost::none;
			
			double scale = totalDesired / totalCurrent;
			
			for (InternalItem& item: items) {
				const Item& i = item.item;
				item.rank = i.desired - i.current * scale;
			}
			
			return std::min_element(items.begin(), items.end(),
									[](const InternalItem& a, const InternalItem& b) {
										return a.rank > b.rank;
									})->item.key;
		}
		
	};
}
