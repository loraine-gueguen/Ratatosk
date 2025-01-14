#include "Correction.hpp"

pair<vector<Path<UnitigData>>, vector<Path<UnitigData>>> extractSemiWeakPaths(	const Correct_Opt& opt, const string& s, const WeightsPairID& w_pid,
																				const pair<size_t, const_UnitigMap<UnitigData>>& um_solid_start,
																				const pair<size_t, const_UnitigMap<UnitigData>>& um_solid_end, 
																				const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_um_weak, size_t i_weak,
																				const bool long_read_correct, const uint64_t hap_id) {

	auto customSort = [](const pair<Path<UnitigData>, size_t>& p1, const pair<Path<UnitigData>, size_t>& p2){

		return (p1.first.back().mappedSequenceToString() < p2.first.back().mappedSequenceToString());
	};

	pair<vector<Path<UnitigData>>, vector<Path<UnitigData>>> paths;
	vector<pair<Path<UnitigData>, size_t>> paths1, paths2;

	const bool no_end = um_solid_end.second.isEmpty;

	const size_t step_sz = opt.k;
    const size_t pos_um_solid2 = (no_end ? s.length() - opt.k : um_solid_end.first);
    const size_t len_weak_region = (pos_um_solid2 - um_solid_start.first) + opt.k;

    const size_t max_len_weak_region = long_read_correct ? opt.max_len_weak_region2 : opt.max_len_weak_region1;
	const size_t max_paths = 512;

	const uint64_t undetermined_hap_id = 0xffffffffffffffffULL;

	size_t next_weak_pos;

    bool begin = true;
    bool end = false;

    Path<UnitigData> tmp;

    tmp.extend(um_solid_start.second, string(um_solid_start.second.len + opt.k - 1, getQual(1.0)));
	paths1.push_back({tmp, um_solid_start.first});

	while ((i_weak < v_um_weak.size()) && (v_um_weak[i_weak].first < um_solid_start.first)) ++i_weak;

	if (i_weak < v_um_weak.size()) next_weak_pos = max(v_um_weak[i_weak].first, um_solid_start.first + opt.k);

	while (!paths1.empty() && !end) {

		pair<vector<Path<UnitigData>>, bool> g_prev_path;

		if (i_weak < v_um_weak.size()){

			while ((i_weak < v_um_weak.size()) && (v_um_weak[i_weak].first < (pos_um_solid2 - opt.k)) && (v_um_weak[i_weak].first < next_weak_pos)) ++i_weak;
		}
		else i_weak = v_um_weak.size();

        sort(paths1.begin(), paths1.end(), customSort);

        end = ((i_weak == v_um_weak.size()) || (v_um_weak[i_weak].first >= (pos_um_solid2 - opt.k)));

        if (end){ // Next semi-solid hit is after the target solid kmer in the query

    		for (size_t i = 0; i < paths1.size(); ++i){

    			const pair<Path<UnitigData>, size_t>& p = paths1[i];
    			const size_t l_len_weak_region = (pos_um_solid2 - p.second) + opt.k;

            	if ((i == 0) || (paths1[i].first.back() != paths1[i-1].first.back())){

            		g_prev_path = {vector<Path<UnitigData>>(), false};

            		const const_UnitigMap<UnitigData>& um_start = (begin ? um_solid_start.second : p.first.back());

					if (no_end){

	        			if (l_len_weak_region <= (max_len_weak_region / 2)) g_prev_path = explorePathsBFS(opt, s.c_str() + p.second, l_len_weak_region, w_pid,
	        																							(begin ? um_solid_start.second : p.first.back()), long_read_correct, hap_id);
	        		}
	        		else if (l_len_weak_region <= max_len_weak_region) {

	        			g_prev_path = explorePathsBFS2(	opt, s.c_str() + p.second, l_len_weak_region, w_pid, um_start, um_solid_end.second, long_read_correct, hap_id);
	        		}
            	}

            	if (g_prev_path.second && !g_prev_path.first.empty()){

            		for (auto& p_tmp : g_prev_path.first){

            			tmp = p.first;

            			tmp.merge(p_tmp);

            			pair<Path<UnitigData>, size_t> toPush = {move(tmp), pos_um_solid2};

            			paths2.push_back(move(toPush));
            		}
            	}
            	else paths.second.push_back(p.first);
            }
        }
        else {

    		for (size_t i = 0; i < paths1.size(); ++i){

    			const pair<Path<UnitigData>, size_t>& p = paths1[i];

	            const size_t l_len_weak_region = (v_um_weak[i_weak].first - p.second) + opt.k;

	            if ((i == 0) || (paths1[i].first.back() != paths1[i-1].first.back())){

            		g_prev_path = {vector<Path<UnitigData>>(), false};

            		const const_UnitigMap<UnitigData>& um_start = (begin ? um_solid_start.second : p.first.back());

	        		if (l_len_weak_region <= max_len_weak_region){

	        			g_prev_path = explorePathsBFS2(	opt, s.c_str() + p.second, l_len_weak_region, w_pid, um_start, v_um_weak[i_weak].second, long_read_correct, hap_id);
            		}
	            }

            	if (g_prev_path.second && !g_prev_path.first.empty()){

            		for (const auto& p_tmp : g_prev_path.first){

            			tmp = p.first;

            			tmp.merge(p_tmp);

            			pair<Path<UnitigData>, size_t> toPush = {move(tmp), v_um_weak[i_weak].first};

            			paths2.push_back(move(toPush));
            		}
            	}
            	else paths.second.push_back(p.first);
            }

            next_weak_pos = v_um_weak[i_weak].first + opt.k;
		}

		begin = false;
        paths1 = move(paths2);

		if (!end && (paths1.size() > max_paths)) {

			size_t best_id = 0;

			vector<const Path<UnitigData>*> v_ptr;

			for (const auto& p : paths1) v_ptr.push_back(&(p.first));

			best_id = selectBestPrefixAlignment(s.c_str() + um_solid_start.first, len_weak_region, v_ptr).first;

		    paths2.push_back(paths1[best_id]);

		    paths1 = move(paths2);
		}
	}

	for (auto& p : paths1) paths.first.push_back(move(p.first));

	return paths;
}

pair<string, string> correctSequence(	const CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt,
										const string& s_fw, const string& q_fw,
										const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_um_solid, const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_um_weak,
										const bool long_read_correct, const Roaring* all_partitions, const uint64_t hap_id, const pair<HapReads, HapReads>& hap_reads,
										const size_t max_km_cov) {

	if ((s_fw.length() <= dbg.getK()) || v_um_solid.empty() || (v_um_solid.size() == s_fw.length() - opt.k + 1)) {

		if (long_read_correct) return {s_fw, q_fw};
		else if (v_um_solid.size() == s_fw.length() - opt.k + 1) return {s_fw, string(s_fw.length(), getQual(1.0))};

		return {s_fw, string(s_fw.length(), getQual(0.0))};
	}

	const size_t seq_len = s_fw.length();

	const string s_bw(reverse_complement(s_fw));

	const size_t max_len_weak_anchors = long_read_correct ? opt.max_len_weak_region2 : opt.max_len_weak_region1;

	const uint64_t undetermined_hap_id = 0xffffffffffffffffULL;

	const PairID dummy_pids;
	const PairID& phased_reads = (hap_id == undetermined_hap_id) ? dummy_pids : hap_reads.first.hap2phasedReads[hap_id];

	const char q_min = getQual(0.0);
	const char q_max = getQual(1.0);

	string q_bw = q_fw;

	size_t prev_pos = v_um_solid[0].first;
	size_t i_solid = 0;
	size_t i_weak = 0;

	stringstream corrected_s;
	stringstream corrected_q;

	vector<pair<size_t, const_UnitigMap<UnitigData>>> v_um_solid_rev(v_um_solid);
	vector<pair<size_t, const_UnitigMap<UnitigData>>> v_um_weak_rev(v_um_weak);

	reverse(v_um_solid_rev.begin(), v_um_solid_rev.end());
	reverse(v_um_weak_rev.begin(), v_um_weak_rev.end());
	reverse(q_bw.begin(), q_bw.end());

	for (auto& p : v_um_solid_rev){

		p.first = seq_len - p.first - opt.k;
		p.second.strand = !p.second.strand;
	}

	for (auto& p : v_um_weak_rev){

		p.first = seq_len - p.first - opt.k;
		p.second.strand = !p.second.strand;
	}

	auto chooseColors = [&](const unordered_map<const SharedPairID*, pair<const PairID*, bool>>& s_pid_s,
							const unordered_map<const SharedPairID*, pair<const PairID*, bool>>& s_pid_e,
							const unordered_map<const SharedPairID*, pair<const PairID*, bool>>& s_pid_w,
							const pair<HapReads, HapReads>& hap_reads, const uint64_t hap_id,
							WeightsPairID& w_pid) {

		vector<const unordered_map<const SharedPairID*, pair<const PairID*, bool>>*> v_s_pid;

		unordered_set<const SharedPairID*> s_spid;

		PairID a_pid[6];

		unordered_set<const PairID*> s_pid_ptr[6];

		v_s_pid.push_back(&s_pid_w);
		v_s_pid.push_back(&s_pid_e);
		v_s_pid.push_back(&s_pid_s);

		for (size_t i = 0; i < v_s_pid.size(); ++i) {

			const unordered_map<const SharedPairID*, pair<const PairID*, bool>>& s_pid = *(v_s_pid[i]);

			for (const auto& it : s_pid){

				const int shift = i + (static_cast<size_t>(it.second.second) * 3);

				const pair<const PairID*, const PairID*> p_pids = it.first->getPairIDs();

				if (p_pids.first != nullptr) s_pid_ptr[shift].insert(p_pids.first);
				else if (p_pids.second != nullptr) s_pid_ptr[shift].insert(p_pids.second);

				if (it.first->cardinality() >= opt.min_cov_vertices) s_spid.insert(it.first);
			}
		}

		for (int i = 5; i >= 0; --i) {

			const vector<const PairID*> v_pid_ptr(s_pid_ptr[i].begin(), s_pid_ptr[i].end());

			a_pid[i] = PairID::fastunion(v_pid_ptr.size(), &(v_pid_ptr.front()));

			if (hap_id != undetermined_hap_id) a_pid[i] &= phased_reads;

			a_pid[i] = a_pid[i].forceRoaringInternal();
			a_pid[i].runOptimize();
		}

		{
			const PairID a_pid_pos[3] = {a_pid[0] | a_pid[3], a_pid[1] | a_pid[4], a_pid[2] | a_pid[5]};

			const PairID a_pid_01 = (a_pid_pos[0] & a_pid_pos[1]);
			const PairID a_pid_12 = (a_pid_pos[1] & a_pid_pos[2]);
			const PairID a_pid_02 = (a_pid_pos[0] & a_pid_pos[2]);

			const PairID* a_pid_nobranch_ptr[3] = {&a_pid[3], &a_pid[4], &a_pid[5]};

			PairID a_pid_inter2, a_pid_inter3;
			PairID a_pid_branch, a_pid_nobranch, a_pid_nobranch_cpy;

			a_pid_nobranch = PairID::fastunion(3, a_pid_nobranch_ptr);
			a_pid_nobranch_cpy = a_pid_nobranch;

			{
				const size_t cov = 30;

				size_t nb_unselected = s_spid.size();

				PairID a_pid2[6];

				vector<pair<const SharedPairID*, int>> v_spids;

				for (const auto spid_ptr : s_spid) v_spids.push_back({spid_ptr, min(cov, spid_ptr->cardinality())});

			    auto cmpCard = [](const pair<const SharedPairID*, int>& a, const pair<const SharedPairID*, int>& b) {

			    	return (a.first->cardinality() < b.first->cardinality());
			    };

			    sort(v_spids.begin(), v_spids.end(), cmpCard);

				for (int i = 5; i >= 0; --i) {

					if (nb_unselected > 0) {

						if (i == 5) {

							a_pid_inter3 = a_pid_01 & a_pid_12;

							a_pid2[5] = a_pid_nobranch & a_pid_inter3; // Contains reads mapping to non-branching unitigs and occurring before, after and in the region to correct
							a_pid2[5].runOptimize();
						}
						else if (i == 4) {

							const PairID* a_pid_ptr[3] = {&a_pid_01, &a_pid_12, &a_pid_02};

							a_pid_inter2 = PairID::fastunion(3, a_pid_ptr);
							a_pid_nobranch -= a_pid2[5];

							a_pid2[4] = a_pid_nobranch & a_pid_inter2; // Contains reads mapping to non-branching unitigs and occurring before and after the region to correct
							a_pid2[4].runOptimize();
						}
						else if (i == 3) {

							a_pid_nobranch -= a_pid2[4];

							a_pid2[3] = move(a_pid_nobranch); // Contains reads mapping to non-branching unitigs and occurring either before, after or in the region to correct
							a_pid2[3].runOptimize();
						}
						else if (i == 2) {

							const PairID* a_pid_ptr[3] = {&a_pid[0], &a_pid[1], &a_pid[2]};

							a_pid_branch = PairID::fastunion(3, a_pid_ptr);
							a_pid_branch -= a_pid_nobranch_cpy;

							a_pid2[2] = a_pid_branch & a_pid_inter3; // Contains reads mapping to branching unitigs and occurring before, after and in the region to correct
							a_pid2[2].runOptimize();
						}
						else if (i == 1) {

							a_pid_branch -= a_pid2[2];

							a_pid2[1] = a_pid_branch & a_pid_inter2; // Contains reads mapping to branching unitigs and occurring before and after the region to correct
							a_pid2[1].runOptimize();
						}
						else {

							a_pid_branch -= a_pid2[1];

							a_pid2[0] = move(a_pid_branch); // Contains reads mapping to branching unitigs and occurring either before, after or in the region to correct
							a_pid2[0].runOptimize();
						}
					}
					else break;

					if (!a_pid2[i].isEmpty()) {

						nb_unselected = 0;

						PairID curr_pid = a_pid2[i];

						for (auto& p_spid : v_spids) {

							if ((p_spid.second > 0) && ((i == 0) || (getNumberSharedPairID(*p_spid.first, curr_pid, 1) >= 1))) {

								const size_t min_cov = min(cov, p_spid.first->cardinality());

								p_spid.second = min_cov - min(getNumberSharedPairID(*p_spid.first, w_pid.all_pids, min_cov), min_cov);

								if (p_spid.second > 0) {

									const size_t all_pids_card = w_pid.all_pids.cardinality();

									const pair<const PairID*, const PairID*> p_pids = p_spid.first->getPairIDs();

									PairID pid;

									if (p_pids.first != nullptr) pid |= *p_pids.first & curr_pid;
									if (p_pids.second != nullptr) pid |= *p_pids.second & curr_pid;

									if (pid.cardinality() > p_spid.second) {

										PairID pid_tmp;
										PairID::const_iterator its = pid.begin();

										for (size_t j = 0; j < p_spid.second; ++j, ++its) pid_tmp.add(*its);

										pid = move(pid_tmp);
									}

									w_pid.all_pids |= pid;
									curr_pid -= pid;
									p_spid.second -= min(static_cast<int>(w_pid.all_pids.cardinality() - all_pids_card), p_spid.second);
								}
							}

							nb_unselected += static_cast<size_t>(p_spid.second > 0);
						}
					}
				}
			}

			// All pids
			{
				w_pid.all_pids = w_pid.all_pids.forceRoaringInternal();
				w_pid.all_pids.runOptimize();
			}

			// Weighted IDs
			{
				w_pid.weighted_pids = w_pid.all_pids & a_pid_nobranch_cpy;
				w_pid.weighted_pids = w_pid.weighted_pids.forceRoaringInternal();
				w_pid.weighted_pids.runOptimize();
			}

			// Unweighted IDs
			{
				w_pid.noWeight_pids = w_pid.all_pids - w_pid.weighted_pids;
				w_pid.noWeight_pids = w_pid.noWeight_pids.forceRoaringInternal();
				w_pid.noWeight_pids.runOptimize();
			}

			// Compute weights
			{
				// Reads mapping to non-branching unitigs weights twice as much as the other reads
				if (!w_pid.weighted_pids.isEmpty()) {

					w_pid.weight = 2.0 * max(static_cast<double>(w_pid.noWeight_pids.cardinality()) / static_cast<double>(w_pid.weighted_pids.cardinality()), 1.0);
				}
				else w_pid.weight = 2.0;

				w_pid.sum_pids_weights = w_pid.weight * static_cast<double>(w_pid.weighted_pids.cardinality()) + static_cast<double>(w_pid.noWeight_pids.cardinality());
			}
		}
	};

	auto correct = [&] (const string& s, const string& q,
						const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_s, const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_w,
						const size_t i_s, const size_t i_w,
						const ResultCorrection* rc, const bool isRevComp) -> ResultCorrection {

		const bool has_end_pt = ((i_s + 1) < v_s.size());

        pair<size_t, const_UnitigMap<UnitigData>> um_solid1 = v_s[i_s]; // Last hit on the leftmost solid region
        pair<size_t, const_UnitigMap<UnitigData>> um_solid2 = has_end_pt ? v_s[i_s + 1] : make_pair(s.length() - opt.k, const_UnitigMap<UnitigData>()); // First hit on the rightmost solid region

        size_t len_weak_region = um_solid2.first - um_solid1.first + opt.k;
        size_t min_cov_vertex = 0xffffffffffffffffULL;

        int64_t min_start = um_solid1.first - opt.insert_sz;
		int64_t min_end = um_solid2.first + opt.insert_sz;

		const char* s_start = s.c_str() + um_solid1.first;
		const char* q_start = q.empty() ? nullptr : q.c_str() + um_solid1.first;

		ResultCorrection res_corrected(len_weak_region);

		string s_corrected, q_corrected;

		vector<pair<size_t, char>> v_ambiguity;
		vector<pair<size_t, const_UnitigMap<UnitigData>>> l_v_w;

		WeightsPairID w_pid;

		auto setUncorrected = [&](const size_t pos, const size_t len, const size_t qual){

			s_corrected = s.substr(pos, len);
			q_corrected = long_read_correct ? q.substr(pos, len) : string(len_weak_region, qual);
		};

		auto addUncorrected = [&](const size_t pos, const size_t len, const size_t qual){

			s_corrected += s.substr(pos, len);
			q_corrected += long_read_correct ? q.substr(pos, len) : string(len_weak_region, qual);
		};

		if (rc == nullptr){
			
			unordered_map<const SharedPairID*, pair<const PairID*, bool>> s_spid_l, s_spid_m, s_spid_r;

			// Left side (left solid anchors)
			{
				size_t nb_branching = 0;

				for (int64_t i_s_s = i_s; (i_s_s >= 0) && (v_s[i_s_s].first > min_start); --i_s_s){

					const const_UnitigMap<UnitigData>& um = v_s[i_s_s].second;
					const UnitigData* ud = um.getData();
					const SharedPairID& p_ids = ud->getPairID();
					const PairID& hap_ids = ud->get_hapID();

					//if (ud->getKmerCoverage(um) < max_km_cov) s_spid_l.insert({&p_ids, {&hap_ids, !ud->isBranching()}});
					if ((ud->getKmerCoverage(um) < max_km_cov) && (!ud->isBranching() || (nb_branching < 5))) {

						const bool unseen_um = s_spid_l.insert({&p_ids, {&hap_ids, !ud->isBranching()}}).second;
						nb_branching += static_cast<size_t>(unseen_um && ud->isBranching());
					}
				}

				{
					const size_t v_w_sz = v_w.size();

					size_t i_w_s = i_w - static_cast<size_t>((i_w != 0) && (i_w >= v_w_sz));

					while ((i_w_s > 0) && (v_w[i_w_s].first > min_start)) --i_w_s;

					for (; (i_w_s < v_w_sz) && (v_w[i_w_s].first < v_s[i_s].first); ++i_w_s){

						const const_UnitigMap<UnitigData>& um = v_w[i_w_s].second;
						const UnitigData* ud = um.getData();
						const SharedPairID& p_ids = ud->getPairID();
						const PairID& hap_ids = ud->get_hapID();

						//if (ud->getKmerCoverage(um) < max_km_cov) s_spid_l.insert({&p_ids, {&hap_ids, !ud->isBranching()}});
						if ((ud->getKmerCoverage(um) < max_km_cov) && (!ud->isBranching() || (nb_branching < 5))) {

							const bool unseen_um = s_spid_l.insert({&p_ids, {&hap_ids, !ud->isBranching()}}).second;
							nb_branching += static_cast<size_t>(unseen_um && ud->isBranching());
						}
					}
				}
			}

			if (has_end_pt) { // Right side if there is one (right solid anchors)

				const size_t v_s_sz = v_s.size();

				size_t nb_branching = 0;

				for (size_t i_s_e = i_s + 1; (i_s_e < v_s_sz) && (v_s[i_s_e].first < min_end); ++i_s_e){

					const const_UnitigMap<UnitigData>& um = v_s[i_s_e].second;
					const UnitigData* ud = um.getData();
					const SharedPairID& p_ids = ud->getPairID();
					const PairID& hap_ids = ud->get_hapID();

					//if (ud->getKmerCoverage(um) < max_km_cov) s_spid_r.insert({&p_ids, {&hap_ids, !ud->isBranching()}});
					if ((ud->getKmerCoverage(um) < max_km_cov) && (!ud->isBranching() || (nb_branching < 5))) {

						const bool unseen_um = s_spid_r.insert({&p_ids, {&hap_ids, !ud->isBranching()}}).second;
						nb_branching += static_cast<size_t>(unseen_um && ud->isBranching());
					}
				}

				{
					const size_t v_w_sz = v_w.size();

					size_t i_w_s = i_w - static_cast<size_t>((i_w != 0) && (i_w >= v_w_sz));

					while ((i_w_s < v_w_sz) && (v_w[i_w_s].first < v_s[i_s + 1].first)) ++i_w_s;

					for (; (i_w_s < v_w_sz) && (v_w[i_w_s].first < min_end); ++i_w_s){

						const const_UnitigMap<UnitigData>& um = v_w[i_w_s].second;
						const UnitigData* ud = um.getData();
						const SharedPairID& p_ids = ud->getPairID();
						const PairID& hap_ids = ud->get_hapID();

						//if (ud->getKmerCoverage(um) < max_km_cov) s_spid_r.insert({&p_ids, {&hap_ids, !ud->isBranching()}});
						if ((ud->getKmerCoverage(um) < max_km_cov) && (!ud->isBranching() || (nb_branching < 5))) {

							const bool unseen_um = s_spid_r.insert({&p_ids, {&hap_ids, !ud->isBranching()}}).second;
							nb_branching += static_cast<size_t>(unseen_um && ud->isBranching());
						}
					}
				}
			}

			if (!v_w.empty()) { // "Middle" (semi-solid anchors in region to correct)

				const size_t pos_end = has_end_pt ? v_s[i_s+1].first : s.length();
				const size_t v_w_sz = v_w.size();

				size_t i_w_s = i_w - static_cast<size_t>((i_w != 0) && (i_w >= v_w_sz));

				// Establish boundaries of semi-solid k-mers we want to use
				while ((i_w_s < v_w_sz) && (v_w[i_w_s].first < v_s[i_s].first)) ++i_w_s;

				for (; (i_w_s < v_w_sz) && (v_w[i_w_s].first < pos_end); ++i_w_s){

					const const_UnitigMap<UnitigData>& um = v_w[i_w_s].second;
					const UnitigData* ud = um.getData();
					const SharedPairID& p_ids = ud->getPairID();
					const PairID& hap_ids = ud->get_hapID();

					l_v_w.push_back(v_w[i_w_s]);

					//if ((ud->getKmerCoverage(um) < max_km_cov) && !ud->isBranching()) s_spid_m.insert({&p_ids, {&hap_ids, !ud->isBranching()}});
					if (ud->getKmerCoverage(um) < max_km_cov) s_spid_m.insert({&p_ids, {&hap_ids, !ud->isBranching()}});
				}
			}

			chooseColors(s_spid_l, s_spid_r, s_spid_m, hap_reads, hap_id, w_pid);

			res_corrected.setWeightsPairID(w_pid);
		}
		else {

			if (!v_w.empty()) { // Select semi-solid anchors but no need to recompute colors

				const size_t pos_end = has_end_pt ? v_s[i_s+1].first : s.length();
				const size_t v_w_sz = v_w.size();

				size_t i_w_s = i_w - static_cast<size_t>((i_w != 0) && (i_w >= v_w_sz));

				// Establish boundaries of semi-solid k-mers we want to use
				while ((i_w_s < v_w_sz) && (v_w[i_w_s].first < v_s[i_s].first)) ++i_w_s;

				for (; (i_w_s < v_w_sz) && (v_w[i_w_s].first < pos_end); ++i_w_s) l_v_w.push_back(v_w[i_w_s]);
			}

			w_pid = rc->getWeightsPairID();
		}

		const size_t card_pids = w_pid.all_pids.cardinality();

		pair<vector<Path<UnitigData>>, vector<Path<UnitigData>>> paths1;

		if (card_pids >= opt.min_cov_vertices) paths1 = extractSemiWeakPaths(opt, s, w_pid, um_solid1, um_solid2, l_v_w, 0, long_read_correct, hap_id);

    	if (paths1.first.empty()){

    		size_t i_w_s = 0;

        	while (paths1.first.empty() && !paths1.second.empty() && !l_v_w.empty() && (card_pids >= opt.min_cov_vertices)){

    			const pair<int, int> align = selectBestPrefixAlignment(s.c_str() + um_solid1.first, len_weak_region, paths1.second, opt.weak_region_len_factor);

    			if (align.first == -1) break;

    			// Search next semi-solid k-mer match to start traversal
    			{  				
	    			size_t next_pos = um_solid1.first + align.second + opt.k;

	    			while ((i_w_s < l_v_w.size()) && (l_v_w[i_w_s].first < next_pos)) ++i_w_s;

	    			if ((i_w_s >= l_v_w.size()) || (l_v_w[i_w_s].first >= um_solid2.first - opt.k) || ((l_v_w[i_w_s].first - um_solid1.first) >= max_len_weak_anchors)) break;
    			}

				const Path<UnitigData>::PathOut path_out = paths1.second[align.first].toStringVector();
				const vector<pair<size_t, char>> v_amb = getAmbiguityVector(path_out, opt.k);

				for (const auto p : v_amb) v_ambiguity.push_back({s_corrected.length() + p.first, p.second});

				s_corrected += path_out.toString() + s.substr(um_solid1.first + align.second + 1, l_v_w[i_w_s].first - um_solid1.first - align.second - 1);
				q_corrected += path_out.toQualityString();

				if (long_read_correct) q_corrected += q.substr(um_solid1.first + align.second + 1, l_v_w[i_w_s].first - um_solid1.first - align.second - 1);
				else q_corrected += string(l_v_w[i_w_s].first - um_solid1.first - align.second - 1, q_min);

				res_corrected.addCorrectedPosOldSeq(um_solid1.first - v_s[i_s].first, um_solid1.first + align.second + 1 - v_s[i_s].first);

            	um_solid1 = l_v_w[i_w_s];
            	len_weak_region = um_solid2.first - um_solid1.first + opt.k;

            	paths1 = extractSemiWeakPaths(opt, s, w_pid, um_solid1, um_solid2, l_v_w, i_w_s, long_read_correct, hap_id);
    		}

        	if (!paths1.first.empty()){

        		const pair<int, int> p_align = selectBestAlignment(paths1.first, s.c_str() + um_solid1.first, len_weak_region);
        		const Path<UnitigData>::PathOut path_out = paths1.first[p_align.first].toStringVector();
        		const vector<pair<size_t, char>> v_amb = getAmbiguityVector(path_out, opt.k);

        		for (const auto p : v_amb) v_ambiguity.push_back({s_corrected.length() + p.first, p.second});

        		s_corrected += path_out.toString();
        		q_corrected += path_out.toQualityString();

				res_corrected.addCorrectedPosOldSeq(um_solid1.first - v_s[i_s].first, um_solid2.first - v_s[i_s].first + opt.k);
        	}
        	else if (!paths1.second.empty()){

        		const pair<int, int> align = selectBestPrefixAlignment(s.c_str() + um_solid1.first, len_weak_region, paths1.second, opt.weak_region_len_factor);

        		if (align.first == -1) {

        			addUncorrected(um_solid1.first, len_weak_region, q_min);
        		}
        		else {

        			const Path<UnitigData>::PathOut path_out = paths1.second[align.first].toStringVector();
        			const vector<pair<size_t, char>> v_amb = getAmbiguityVector(path_out, opt.k);

		        	for (const auto p : v_amb) v_ambiguity.push_back({s_corrected.length() + p.first, p.second});

		        	s_corrected += path_out.toString() + s.substr(um_solid1.first + align.second + 1, len_weak_region - align.second - 1);
		        	q_corrected += path_out.toQualityString();

		        	if (long_read_correct) q_corrected += q.substr(um_solid1.first + align.second + 1, len_weak_region - align.second - 1);
		        	else q_corrected += string(len_weak_region - align.second - 1, q_min);

					res_corrected.addCorrectedPosOldSeq(um_solid1.first - v_s[i_s].first, um_solid1.first + align.second + 1 - v_s[i_s].first);
				}
    		}
    		else if (!s_corrected.empty()) {

    			addUncorrected(um_solid1.first, len_weak_region, q_min);
    		}
    		else {

    			setUncorrected(v_s[i_s].first, len_weak_region, q_min);
    		}
    	}
    	else {

    		const pair<int, int> p_align = selectBestAlignment(paths1.first, s.c_str() + um_solid1.first, len_weak_region);
    		const Path<UnitigData>::PathOut path_out = paths1.first[p_align.first].toStringVector();
    		const vector<pair<size_t, char>> v_amb = getAmbiguityVector(path_out, opt.k);

    		for (const auto p : v_amb) v_ambiguity.push_back(p);

    		s_corrected = path_out.toString();
    		q_corrected = path_out.toQualityString();

			res_corrected.addCorrectedPosOldSeq(0, len_weak_region);
    	}

    	//if (long_read_correct) q_corrected.replace(0, opt.k, q, v_s[i_s].first, opt.k);
    	//else q_corrected.replace(0, opt.k, opt.k, q_max);

    	fixAmbiguity(dbg, opt, w_pid, s_corrected, q_corrected, s_start, q_start, res_corrected.getLengthOldSequence(), hap_id, v_ambiguity);

    	if (res_corrected.getNbCorrectedPosOldSeq() == res_corrected.getLengthOldSequence()){

	    	const Kmer end_s(s.c_str() + s.length() - opt.k);
	    	const Kmer end_s_corr(s_corrected.c_str() + s_corrected.length() - opt.k);

	    	// Region has all its bases corrected, first and last k-mers are solid -> region is corrected
	    	if (end_s == end_s_corr) res_corrected.setCorrected();
	    }
	    
	    if (!res_corrected.isCorrected()) {

    		// Corrected region might span the weak region too far
			EdlibAlignConfig config(edlibNewAlignConfig(-1, EDLIB_MODE_SHW, EDLIB_TASK_DISTANCE, edlib_iupac_alpha, sz_edlib_iupac_alpha));
			EdlibAlignResult align = edlibAlign(s_start, um_solid2.first - v_s[i_s].first + opt.k, s_corrected.c_str(), s_corrected.length(), config);

			if (align.editDistance >= 0){

				size_t end_location = align.endLocations[0];

				for (size_t j = 1; j < align.numLocations; ++j){

					if (align.endLocations[j] > end_location) end_location = align.endLocations[j];
				}

				s_corrected = s_corrected.substr(0, end_location + 1);
				q_corrected = q_corrected.substr(0, end_location + 1);
			}

			edlibFreeAlignResult(align);
    	}

    	res_corrected.setSequence(move(s_corrected));
    	res_corrected.setQuality(move(q_corrected));

		return res_corrected;
	};

	/*if (long_read_correct) {
		
		unordered_map<const SharedPairID*, pair<const PairID*, bool>> s_pid_solid;

		for (size_t i_s_e = 0; i_s_e < v_um_solid.size(); ++i_s_e){

			const const_UnitigMap<UnitigData>& um = v_um_solid[i_s_e].second;
			const UnitigData* ud = um.getData();

			const SharedPairID& p_ids = ud->getPairID();
			const PairID& hap_ids = ud->get_hapID();

			if (((i_s_e == 0) || !um.isSameReferenceUnitig(v_um_solid[i_s_e - 1].second)) && (ud->getKmerCoverage(um) < max_km_cov) && !ud->isBranching()) {

				s_pid_solid.insert({&p_ids, {&hap_ids, true}});
			}
		}
		
		chooseColors(s_pid_solid, hap_reads, hap_id, lr_w_pid);
	}*/

    if (v_um_solid[0].first != 0){ // Read starts with a semi-solid region (no left solid region)

    	// Do not correct if second pass of correction and sequence has already max correction score from first pass
    	if (!long_read_correct || (q_fw.length() == 0) || !hasMinQual(s_fw, q_fw, 0, v_um_solid[0].first + opt.k, q_max)){

	    	const size_t i_solid_rev = v_um_solid_rev.size() - 1;

	    	size_t i_weak_rev = v_um_weak_rev.size();

	    	while ((i_weak_rev > 0) && (v_um_weak_rev[i_weak_rev - 1].first > v_um_solid_rev[i_solid_rev].first)) --i_weak_rev;

	    	const ResultCorrection bw_corrected = correct(s_bw, q_fw, v_um_solid_rev, v_um_weak_rev, i_solid_rev, i_weak_rev, nullptr, true).reverseComplement();

	    	corrected_s << bw_corrected.getSequence().substr(0, bw_corrected.getSequence().length() - opt.k);
	    	corrected_q << bw_corrected.getQuality().substr(0, bw_corrected.getQuality().length() - opt.k);
    	}
    	else { // output uncorrected

    		corrected_s << s_fw.substr(0, v_um_solid[0].first);
    		corrected_q << (long_read_correct ? q_fw.substr(0, v_um_solid[0].first) : string(v_um_solid[0].first, q_min));
    	}
    }

	while (i_solid < v_um_solid.size() - 1) {

		while ((i_weak < v_um_weak.size()) && (v_um_weak[i_weak].first < v_um_solid[i_solid].first)) ++i_weak;

	    if (v_um_solid[i_solid].first != (v_um_solid[i_solid + 1].first - 1)){ // Start of a semi-solid region of the read

	    	bool isUncorrected = false;

	    	// Do not correct during second pass if subsequence has already the maximum correction score on all its bases
	    	if (!long_read_correct || (q_fw.length() == 0) || !hasMinQual(s_fw, q_fw, v_um_solid[i_solid].first, v_um_solid[i_solid + 1].first + opt.k, q_max)){

		        const const_UnitigMap<UnitigData>& start_um = v_um_solid[i_solid].second;
		        const const_UnitigMap<UnitigData>& end_um = v_um_solid[i_solid + 1].second;

		        // Check same unitig and same strand
		        bool sameUnitig = (start_um.getUnitigHead() == end_um.getUnitigHead()) && (start_um.strand == end_um.strand);

		        if (sameUnitig && !start_um.getData()->isShortCycle()) { // Is not in a short cycle

		    		const size_t min_pos = min(start_um.dist, end_um.dist);
		    		const size_t max_pos = max(start_um.dist, end_um.dist);

		    		const size_t len_query_km = v_um_solid[i_solid + 1].first - v_um_solid[i_solid].first;
		    		const size_t len_unitig_km = max_pos - min_pos;

					const size_t min_len_unitig_km = getMinMaxLength(len_unitig_km, opt.weak_region_len_factor).first;
					const size_t max_len_unitig_km = getMinMaxLength(len_unitig_km, opt.weak_region_len_factor).second;

					const Kmer start_km = start_um.getUnitigHead();

		        	// Start and end positions are compatible
		        	sameUnitig = sameUnitig && ((start_um.strand && (start_um.dist < end_um.dist)) || (!start_um.strand && (start_um.dist > end_um.dist)));
		        	// Check that lengths are compatible
		        	sameUnitig = sameUnitig && (len_query_km >= min_len_unitig_km) && (len_query_km <= max_len_unitig_km);
		        	// Check that haploblocks and haplotypes are compatible
		        	sameUnitig = sameUnitig && ((hap_id == undetermined_hap_id) || isValidHap(start_um.getData()->get_hapID(), hap_id));

		        	if (sameUnitig){

	        			const_UnitigMap<UnitigData> um_sub = start_um;

	        			um_sub.dist = min_pos;
	        			um_sub.len = len_unitig_km + 1;

	        			const string s_um_sub = um_sub.mappedSequenceToString();

		        		corrected_s << s_fw.substr(prev_pos, v_um_solid[i_solid].first - prev_pos) + s_um_sub.substr(0, s_um_sub.length() - opt.k);

	        			if (long_read_correct){

	        				const size_t buff = (s_um_sub.length() >= (2 * opt.k)) ? opt.k : (s_um_sub.length() - opt.k);

	        				corrected_q << q_fw.substr(prev_pos, v_um_solid[i_solid].first - prev_pos + buff);
	        				
	        				if ((s_um_sub.length() - buff - opt.k) > 0) corrected_q << string(s_um_sub.length() - buff - opt.k, q_max);
	        			}
	        			else corrected_q << string((v_um_solid[i_solid].first - prev_pos) + (s_um_sub.length() - opt.k), q_max);
	        		}
					else isUncorrected = true; // No correction
		        }
				else if (v_um_solid[i_solid + 1].first >= (v_um_solid[i_solid].first + opt.k)) {

		        	const ResultCorrection fw_corrected = correct(s_fw, q_fw, v_um_solid, v_um_weak, i_solid, i_weak, nullptr, false);

		        	if (fw_corrected.isCorrected()) {

		        		const size_t l_solid = v_um_solid[i_solid].first - prev_pos;

		        		const string corrected_subseq_fw = s_fw.substr(prev_pos, l_solid) + fw_corrected.getSequence();
		        		const string corrected_subqual_fw = (long_read_correct ? q_fw.substr(prev_pos, l_solid) : string(l_solid, q_max)) + fw_corrected.getQuality();

		        		corrected_s << corrected_subseq_fw.substr(0, corrected_subseq_fw.length() - opt.k);
		        		corrected_q << corrected_subqual_fw.substr(0, corrected_subqual_fw.length() - opt.k);
		        	}
		        	else {

			    		size_t i_solid_bw = v_um_solid_rev.size() - i_solid - 2;
			    		size_t i_weak_bw = v_um_weak_rev.size() - i_weak;

			    		while ((i_weak_bw > 0) && (v_um_weak_rev[i_weak_bw - 1].first > v_um_solid_rev[i_solid_bw].first)) --i_weak_bw;

			        	const ResultCorrection bw_corrected = correct(s_bw, q_bw, v_um_solid_rev, v_um_weak_rev, i_solid_bw, i_weak_bw, &fw_corrected, true).reverseComplement();

			        	if (bw_corrected.isCorrected()){

			        		const size_t l_solid = (s_bw.length() - v_um_solid_rev[i_solid_bw + 1].first - opt.k) - prev_pos;

			        		const string corrected_subseq_bw = s_fw.substr(prev_pos, l_solid) + bw_corrected.getSequence();
			        		const string corrected_subqual_bw = (long_read_correct ? q_fw.substr(prev_pos, l_solid) : string(l_solid, q_max)) + bw_corrected.getQuality();

			        		corrected_s << corrected_subseq_bw.substr(0, corrected_subseq_bw.length() - opt.k);
			        		corrected_q << corrected_subqual_bw.substr(0, corrected_subqual_bw.length() - opt.k);
			        	}
			        	else {

			        		string l_ref = s_fw.substr(v_um_solid[i_solid].first, v_um_solid[i_solid + 1].first - v_um_solid[i_solid].first + opt.k);
			        		
			        		pair<string, string> p_consensus = generateConsensus(&fw_corrected, &bw_corrected, l_ref, opt.weak_region_len_factor);

			        		if (p_consensus.first.length() == 0) {

			        			p_consensus.first = move(l_ref);

			        			if (long_read_correct) p_consensus.second = q_fw.substr(v_um_solid[i_solid].first, v_um_solid[i_solid + 1].first - v_um_solid[i_solid].first + opt.k);
			        			else p_consensus.second = string(opt.k, q_max) + string(v_um_solid[i_solid + 1].first - v_um_solid[i_solid].first, q_min);
			        		}

			        		const size_t l_solid = v_um_solid[i_solid].first - prev_pos;

					        const string corrected_subseq_consensus = s_fw.substr(prev_pos, l_solid) + p_consensus.first;
					        const string corrected_subqual_consensus = (long_read_correct ? q_fw.substr(prev_pos, l_solid) : string(l_solid, q_max)) + p_consensus.second;

					        corrected_s << corrected_subseq_consensus.substr(0, corrected_subseq_consensus.length() - opt.k);
					        corrected_q << corrected_subqual_consensus.substr(0, corrected_subqual_consensus.length() - opt.k);
				        }
		        	}
		        }
				else isUncorrected = true; // No correction
        	}
        	else isUncorrected = true; // No correction

        	if (isUncorrected){ // No correction was performed, output uncorrected sequence and quality scores

	        	corrected_s << s_fw.substr(prev_pos, v_um_solid[i_solid + 1].first - prev_pos);

	        	if (long_read_correct) corrected_q << q_fw.substr(prev_pos, v_um_solid[i_solid + 1].first - prev_pos);
	        	else {

	        		corrected_q << string(v_um_solid[i_solid].first - prev_pos, q_max);

	        		if (v_um_solid[i_solid + 1].first < (v_um_solid[i_solid].first + opt.k)) corrected_q << string(v_um_solid[i_solid + 1].first - v_um_solid[i_solid].first, q_max);
	        		else corrected_q << string(opt.k, q_max) << string(v_um_solid[i_solid + 1].first - v_um_solid[i_solid].first - opt.k, q_min);
	        	}
        	}

        	prev_pos = v_um_solid[i_solid + 1].first;
        }
        
        ++i_solid;
    }

    if 	((v_um_solid[v_um_solid.size() - 1].first < s_fw.length() - opt.k) && // Semi-solid region at the end of reads, no right anchor
    	(!long_read_correct || (q_fw.length() == 0) || !hasMinQual(s_fw, q_fw, v_um_solid[v_um_solid.size() - 1].first, s_fw.length(), q_max))) {

    	while ((i_weak < v_um_weak.size()) && (v_um_weak[i_weak].first < v_um_solid[i_solid].first)) ++i_weak;

   		const ResultCorrection fw_corrected = correct(s_fw, q_fw, v_um_solid, v_um_weak, i_solid, i_weak, nullptr, false);
   		const size_t l_solid = v_um_solid[i_solid].first - prev_pos;

        corrected_s << s_fw.substr(prev_pos, l_solid) << fw_corrected.getSequence(); // Push right solid region
        corrected_q << (long_read_correct ? q_fw.substr(prev_pos, l_solid) : string(l_solid, q_max)) << fw_corrected.getQuality();
    }
	else {

		corrected_s << s_fw.substr(prev_pos);
		corrected_q << (long_read_correct ? q_fw.substr(prev_pos) : (string(v_um_solid[i_solid].first - prev_pos + opt.k, q_max) + string(s_fw.length() - v_um_solid[i_solid].first - opt.k, q_min)));
	}

    return {corrected_s.str(), corrected_q.str()};
}

unordered_map<size_t, pair<PairID, PairID>, CustomHashSize_t> indexReadIDs(	const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_um_solid,
																			const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_um_weak,
																			const size_t max_km_cov) {

	unordered_map<size_t, pair<PairID, PairID>, CustomHashSize_t> m_id_um;

	const pair<PairID, PairID> empty_pids;

	// Solid
	{
		unordered_set<Kmer, KmerHash> s_km;

		for (size_t i = 0; i < v_um_solid.size(); ++i) {

			const const_UnitigMap<UnitigData>& um = v_um_solid[i].second;

			if (um.getData()->getKmerCoverage(um) < max_km_cov) {

				const SharedPairID& spid = um.getData()->getPairID();

				for (const auto id : spid) {

					const pair<unordered_map<size_t, pair<PairID, PairID>, CustomHashSize_t>::iterator, bool> it = m_id_um.insert(pair<size_t, pair<PairID, PairID>>(id, empty_pids));

					it.first->second.first.add(i);
				}
			}
		}

		for (auto& it : m_id_um) it.second.first.runOptimize();
	}

	// Weak
	{
		unordered_set<Kmer, KmerHash> s_km;

		for (size_t i = 0; i < v_um_weak.size(); ++i) {

			const const_UnitigMap<UnitigData>& um = v_um_weak[i].second;

			if (um.getData()->getKmerCoverage(um) < max_km_cov) {

				const SharedPairID& spid = um.getData()->getPairID();

				for (const auto id : spid) {

					const pair<unordered_map<size_t, pair<PairID, PairID>, CustomHashSize_t>::iterator, bool> it = m_id_um.insert(pair<size_t, pair<PairID, PairID>>(id, empty_pids));

					it.first->second.second.add(i);
				}
			}
		}

		for (auto& it : m_id_um) it.second.second.runOptimize();
	}

	return m_id_um;
}