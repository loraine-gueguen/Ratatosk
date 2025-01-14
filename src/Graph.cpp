#include "Graph.hpp"

pair<vector<pair<size_t, const_UnitigMap<UnitigData>>>, vector<pair<size_t, const_UnitigMap<UnitigData>>>> getSeeds(const Correct_Opt& opt, const CompactedDBG<UnitigData>& dbg,
																													const string& s, const string& q, 
																													const bool long_read_correct, const uint64_t hap_id,
																													unordered_map<Kmer, vector<const_UnitigMap<UnitigData>>, KmerHash>& m_km_um,
																													const bool fill_map) {

    auto comp_pair = [](const pair<size_t, const_UnitigMap<UnitigData>>& p1, const pair<size_t, const_UnitigMap<UnitigData>>& p2) {

        if (p1.first == p2.first) return (p1.second.mappedSequenceToString() < p2.second.mappedSequenceToString());

        return (p1.first < p2.first);
    };

    auto removeEmptyUnitigMap = [](vector<pair<size_t, const_UnitigMap<UnitigData>>>& v) {

    	vector<pair<size_t, const_UnitigMap<UnitigData>>> v2;

    	for (const auto& p : v){

    		if (!p.second.isEmpty) v2.push_back(p);
    	}

    	v = move(v2);
    };

    auto fillMap = [&](const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v) {

		for (const auto& p : v) {

			const Kmer km(s.c_str() + p.first);
			const pair<unordered_map<Kmer, vector<const_UnitigMap<UnitigData>>, KmerHash>::iterator, bool> p_it = m_km_um.insert({km, vector<const_UnitigMap<UnitigData>>()});

			vector<const_UnitigMap<UnitigData>>& l_v = p_it.first->second;

			if (p_it.second) l_v.push_back(p.second);
			else {

				bool same = false;

				for (const auto& um : l_v) same = (same || (um == p.second));

				if (!same) l_v.push_back(p.second);
			}
		}
    };

    if (s.length() <= opt.k) return {vector<pair<size_t, const_UnitigMap<UnitigData>>>(), vector<pair<size_t, const_UnitigMap<UnitigData>>>()};

    // Find all exact or inexact k-mer hits between the query and the graph
    vector<pair<size_t, const_UnitigMap<UnitigData>>> v_um, v_um_solid, v_um_weak;

    /*if (!m_km_um.empty()) { // Some k-mers have already been searched for on a previous round, no need to re-search those

    	string l_s = std::string(s.length(), 'N');

    	Roaring r_noDNApos;

    	vector<pair<size_t, const_UnitigMap<UnitigData>>> l_v_um;

    	for (size_t i = 0; i < s.length(); ++i) {

    		if (!isDNA(s[i])) {

    			const size_t s_pos = i < (k-1) ? 0 : (i-k+1);
    			const size_t e_pos = (i+k >= s.length()) ? s.length() : (i+k);

    			r_noDNApos.addRange(s_pos, e_pos);
    		}
    	}

       	for (size_t i = 0; i < s.length() - opt.k + 1; ++i){

       		if (r_noDNApos.contains(i)) {

	    		const Kmer km(s.c_str() + i);
	    		const unordered_map<Kmer, vector<const_UnitigMap<UnitigData>>, KmerHash>::const_iterator it = m_km_um.find(km);

	    		if (it != m_km_um.end()) {

	    			for (const auto& um : it->second) l_v_um.push_back({i, um});
	    		}
	    		else l_s.replace(i, opt.k, s, i, opt.k);
	    	}
	    	else l_s.replace(i, opt.k, s, i, opt.k);
    	}

    	v_um = dbg.searchSequence(l_s, true, false, false, false, false);

    	if (fill_map) fillMap(v_um); 

    	v_um.insert(v_um.end(), l_v_um.begin(), l_v_um.end());
    }
    else {*/

    	v_um = dbg.searchSequence(s, true, false, false, false, false);
    	//v_um = dbg.searchSequence(s, true, !long_read_correct, !long_read_correct, !long_read_correct, !long_read_correct);

    	if (!long_read_correct) {

       		string l_s = std::string(s.length(), 'N');

       		sort(v_um.begin(), v_um.end(), comp_pair);

       		for (size_t i = 1; i < v_um.size(); ++i) {

       			if (v_um[i].first != (v_um[i-1].first + 1)) {

       				const size_t diff = v_um[i].first - v_um[i-1].first;

       				if (diff >= opt.insert_sz) {

       					//l_s.replace(v_um[i-1].first + 1, diff + opt.k - 2, s, v_um[i-1].first + 1, diff + opt.k - 2);
       					l_s.replace(v_um[i-1].first + opt.k, diff - opt.k, s, v_um[i-1].first + opt.k, diff - opt.k);
       				}
       				/*else {

       					const SharedPairID& spid_l = v_um[i-1].second.getData()->getPairID();
       					const SharedPairID& spid_r = v_um[i].second.getData()->getPairID();

       					if (getNumberSharedPairID(spid_l, spid_r, opt.min_cov_vertices) < opt.min_cov_vertices) {

       						l_s.replace(v_um[i-1].first + opt.k, diff - opt.k, s, v_um[i-1].first + opt.k, diff - opt.k);
       					}
       				}*/
       				else if (diff >= (opt.insert_sz / 2)) {

       					const size_t search_space_len = (opt.insert_sz - diff);

		    			const size_t min_pos_left = (v_um[i-1].first < search_space_len) ? 0 : (v_um[i-1].first - search_space_len);
		    			const size_t max_pos_right = v_um[i].first + search_space_len;

		    			const pair<size_t, const_UnitigMap<UnitigData>>* prev_left = nullptr;
		    			const pair<size_t, const_UnitigMap<UnitigData>>* prev_right = nullptr;

		    			PairID pid_left, pid_right;

		    			size_t i_l = i-1;
		    			size_t i_r = i;

		    			while ((i_l > 0) && (v_um[i_l].first > min_pos_left)){

		    				const const_UnitigMap<UnitigData>& um_left = v_um[i_l].second;

		    				if ((prev_left == nullptr) || !um_left.isSameReferenceUnitig(prev_left->second)) {

			    				if (!um_left.getData()->isBranching()) {

			    					const pair<const PairID*, const PairID*> pids = um_left.getData()->getPairID().getPairIDs();

			    					if (pids.first != nullptr) pid_left |= *pids.first;
			    					if (pids.second != nullptr) pid_left |= *pids.second;
			    				}

			    				prev_left = &v_um[i_l];
		    				}

		    				--i_l;
		    			}

		    			while ((i_r < v_um.size()) && (v_um[i_r].first < max_pos_right)) {

		    				const const_UnitigMap<UnitigData>& um_right = v_um[i_r].second;

		    				if ((prev_right == nullptr) || !um_right.isSameReferenceUnitig(prev_right->second)) {

			    				if (!um_right.getData()->isBranching()){

			    					const pair<const PairID*, const PairID*> pids = um_right.getData()->getPairID().getPairIDs();

			    					if (pids.first != nullptr) pid_right |= *pids.first;
			    					if (pids.second != nullptr) pid_right |= *pids.second;
			    				}

			    				prev_right = &v_um[i_r];
		    				}

		    				++i_r;
		    			}

		    			if (getNumberSharedPairID(pid_left, pid_right, opt.min_cov_vertices) < opt.min_cov_vertices) l_s.replace(v_um[i-1].first + opt.k, diff - opt.k, s, v_um[i-1].first + opt.k, diff - opt.k);
       				}
       			}
       		}

       		if (!v_um.empty()) {

	       		if (v_um.front().first >= (opt.insert_sz/2)) l_s.replace(0, v_um.front().first + opt.k - 1, s, 0, v_um.front().first + opt.k - 1);
	       		if (s.length() - v_um.back().first >= (opt.insert_sz/2)) l_s.replace(v_um.back().first + 1, s.length() - v_um.back().first - 1, s, v_um.back().first + 1, s.length() - v_um.back().first - 1);
       		}

       		vector<pair<size_t, const_UnitigMap<UnitigData>>> v_um_inexact = dbg.searchSequence(l_s, false, true, true, true, true);

       		v_um.insert(v_um.end(), v_um_inexact.begin(), v_um_inexact.end());
    	}

    	//if (fill_map) fillMap(v_um); 
    //}

    sort(v_um.begin(), v_um.end(), comp_pair); // Sort k-mer hits by query position and mapped k-mer similarity

    // Divide k-mer matches into solid and non-solid k-mers
	{
		const size_t v_um_sz = v_um.size();

		const char* s_str = s.c_str();

		for (size_t i = 0; i < v_um_sz; ++i) {

			if ((i == 0) || (v_um[i] != v_um[i-1])){

				if (cstrMatch(v_um[i].second.mappedSequenceToString().c_str(), s_str + v_um[i].first) == opt.k) v_um_solid.push_back(v_um[i]);
				else v_um_weak.push_back(v_um[i]);
			}
		}

		v_um.clear();
	}

	if (v_um_solid.size() >= 2) { // Make sure solid k-mers overlap by k-1

    	for (size_t i = 1; i < v_um_solid.size(); ++i){

    		if ((v_um_solid[i].first != (v_um_solid[i-1].first + 1)) && (v_um_solid[i].first < (v_um_solid[i-1].first + opt.k))) {

    			v_um_solid[i-1].second.isEmpty = true;

    			int64_t j = i-2;

    			while ((j >= 0) && (v_um_solid[j].first == (v_um_solid[j+1].first - 1)) && (v_um_solid[i].first < (v_um_solid[j].first + opt.k))){

    				v_um_solid[j--].second.isEmpty = true;
    			}
    		}
    	}

    	removeEmptyUnitigMap(v_um_solid);
	}

	if (!v_um_weak.empty()) v_um_weak = keep_non_overlap(opt, s.c_str(), v_um_weak);

	// TEST
	/*if (long_read_correct) {

		struct CustomHashPair {

		    size_t operator()(const pair<size_t, size_t>& p) const {

		        return wyhash(&p, sizeof(pair<size_t, size_t>), 0, _wyp);
		    }
		};

	    auto comp_pair = [](const pair<pair<size_t, size_t>, size_t>& p1, const pair<pair<size_t, size_t>, size_t>& p2) {

	        return (p1.second < p2.second);
	    };

		unordered_map<size_t, vector<size_t>> m_pids;
		unordered_map<pair<size_t, size_t>, size_t, CustomHashPair> m_tup;

		vector<pair<pair<size_t, size_t>, size_t>> v_tup;
		vector<size_t> v_all_pids;

		for (size_t i = 0; i < v_um_solid.size(); ++i) { // Create index of <read ID, vector<position solid UnitigMap>> for SNP candidates

			const const_UnitigMap<UnitigData>& um = v_um_solid[i].second;

			if (((i==0) || !um.isSameReferenceUnitig(v_um_solid[i-1].second)) && !um.getData()->isBranching()) {

				const SharedPairID& spid = um.getData()->getPairID();

				for (const auto id : spid) m_pids.insert(pair<size_t, vector<size_t>>(id, vector<size_t>())).first->second.push_back(i);

				v_all_pids.push_back(i);
			}
		}

		if (v_all_pids.size() >= 2) {

			for (const auto& pid_occ : m_pids) { // Create tuples of adjacent SNP candidates for that read with corresponding coverage

				for (size_t i = 0; i < pid_occ.second.size() - 1; ++i) {

					const pair<size_t, size_t> p = {pid_occ.second[i], pid_occ.second[i+1]};

					m_tup.insert(pair<pair<size_t, size_t>, size_t>(p, 0)).first->second += 1;
				}
			}

			for (size_t i = 0; i < v_all_pids.size() - 1; ++i) {

				const unordered_map<pair<size_t, size_t>, size_t, CustomHashPair>::const_iterator it = m_tup.find({v_all_pids[i], v_all_pids[i+1]});

				if (it != m_tup.end()) v_tup.push_back(*it);
			}

			sort(v_tup.begin(), v_tup.end(), comp_pair);

			for (size_t i = 0; i < 0.05 * v_tup.size(); ++i) { // Delete the bottom 5% of tuple of SNP candidate with the least coverage

				const_UnitigMap<UnitigData>& um_a = v_um_solid[v_tup[i].first.first].second;
				const_UnitigMap<UnitigData>& um_b = v_um_solid[v_tup[i].first.second].second;

				for (size_t j = v_tup[i].first.first + 1; j < v_um_solid.size(); ++j) {

					if (v_um_solid[j].second.isSameReferenceUnitig(um_a)) v_um_solid[j].second.isEmpty = true;
					else break;
				}

				for (size_t j = v_tup[i].first.second + 1; j < v_um_solid.size(); ++j) {

					if (v_um_solid[j].second.isSameReferenceUnitig(um_b)) v_um_solid[j].second.isEmpty = true;
					else break;
				}

				um_a.isEmpty = true;
				um_b.isEmpty = true;
			}
		}

		removeEmptyUnitigMap(v_um_solid);
	}*/

	// Runs of solid k-mers must be consistent:
	// - Kmer i+1 must be on same unitig as k-mer i or on a successor unitig
	// - In the latter case, both unitigs must share a minimum number of colors
	{
    	for (size_t i = 1; i < v_um_solid.size(); ++i){ // Make sure solid regions are color-consistent

    		if ((v_um_solid[i].first - v_um_solid[i-1].first) == 1) {

    			const_UnitigMap<UnitigData>& um_left = v_um_solid[i-1].second;
    			const_UnitigMap<UnitigData>& um_right = v_um_solid[i].second;

    			if (!um_left.isEmpty && !um_right.isEmpty && !um_left.isSameReferenceUnitig(um_right)){

    				const string s_tail_left = (um_left.strand ? um_left.getUnitigTail() : um_left.getUnitigHead().twin()).toString();
    				const string s_head_right = (um_right.strand ? um_right.getUnitigHead() : um_right.getUnitigTail().twin()).toString();

    				bool invalidAnchors = (s_tail_left.substr(1, opt.k-1) != s_head_right.substr(0, opt.k-1));

    				if (!invalidAnchors) invalidAnchors = (getNumberSharedPairID(um_left.getData()->getPairID(), um_right.getData()->getPairID(), opt.min_cov_vertices) < opt.min_cov_vertices);

    				if (invalidAnchors) {

		    			size_t i_l = i-1;
		    			size_t i_r = i+1;

		    			i_l -= (i_l != 0);

		    			while ((i_l > 0) && (v_um_solid[i_l].first == (v_um_solid[i_l+1].first - 1)) && v_um_solid[i_l].second.isSameReferenceUnitig(um_left)){

		    				v_um_solid[i_l].second.isEmpty = true;
		    				--i_l;
		    			}

		    			while ((i_r < v_um_solid.size()) && (v_um_solid[i_r].first == (v_um_solid[i_r-1].first + 1)) && v_um_solid[i_r].second.isSameReferenceUnitig(um_right)){

		    				v_um_solid[i_r].second.isEmpty = true;
		    				++i_r;
		    			}

		    			um_left.isEmpty = true;
		    			um_right.isEmpty = true;
    				}
    			}
    		}
    	}

    	removeEmptyUnitigMap(v_um_solid);
    }

    // Make sure solid regions are color-consistent
    /*{
    	for (size_t i = 1; i < v_um_solid.size(); ++i){

    		const size_t diff_pos = v_um_solid[i].first - v_um_solid[i-1].first;

    		if ((diff_pos > 1) && (diff_pos < (opt.insert_sz/2))) {

    			bool shareGlobalPairID = false;

    			// Check first for intersection of shared PairIDs
    			{
	    			unordered_set<const PairID*> us_gpid;

	    			const const_UnitigMap<UnitigData>* prev_um_left = &(v_um_solid[i-1].second);
	    			const const_UnitigMap<UnitigData>* prev_um_right = &(v_um_solid[i].second);

	    			const PairID* g_pids_l = prev_um_left->getData()->getPairID().getPairIDs().first;
	    			const PairID* g_pids_r = prev_um_right->getData()->getPairID().getPairIDs().first;

	    			size_t i_l = i-1;
	    			size_t i_r = i+1;

	    			i_l -= (i_l != 0);

	    			if (g_pids_l != nullptr) us_gpid.insert(g_pids_l);

	    			while ((i_l > 0) && (v_um_solid[i_l].first == (v_um_solid[i_l+1].first - 1))){

	    				if (!v_um_solid[i_l].second.isSameReferenceUnitig(*prev_um_left)) {

	    					const PairID* g_pids = v_um_solid[i_l].second.getData()->getPairID().getPairIDs().first;

	    					if (g_pids != nullptr) us_gpid.insert(g_pids);

		    				prev_um_left = &(v_um_solid[i_l].second);
	    				}

	    				--i_l;
	    			}

	    			shareGlobalPairID = ((g_pids_r != nullptr) && (us_gpid.find(g_pids_r) != us_gpid.end()) && (g_pids_r->cardinality() >= opt.min_cov_vertices));

	    			while ((i_r < v_um_solid.size()) && (v_um_solid[i_r].first == (v_um_solid[i_r-1].first + 1)) && !shareGlobalPairID){

	    				if (!v_um_solid[i_r].second.isSameReferenceUnitig(*prev_um_right)) {

		    				const PairID* g_pids = v_um_solid[i_r].second.getData()->getPairID().getPairIDs().first;

		    				shareGlobalPairID = shareGlobalPairID || ((g_pids != nullptr) && (us_gpid.find(g_pids) != us_gpid.end()) && (g_pids->cardinality() >= opt.min_cov_vertices));
		    				prev_um_right = &(v_um_solid[i_r].second);
	    				}

	    				++i_r;
	    			}
	    		}

	    		if (!shareGlobalPairID) { // No intersection of shared PairIDs

	    			const const_UnitigMap<UnitigData>* prev_um_left = &(v_um_solid[i-1].second);
	    			const const_UnitigMap<UnitigData>* prev_um_right = &(v_um_solid[i].second);

	    			SharedPairID pid_left = v_um_solid[i-1].second.getData()->getPairID();
	    			SharedPairID pid_right = v_um_solid[i].second.getData()->getPairID();

	    			size_t i_l = i-1;
	    			size_t i_r = i+1;

	    			i_l -= (i_l != 0);

	    			while ((i_l > 0) && (v_um_solid[i_l].first == (v_um_solid[i_l+1].first - 1))){

	    				if (!v_um_solid[i_l].second.isSameReferenceUnitig(*prev_um_left)) {

		    				pid_left |= v_um_solid[i_l].second.getData()->getPairID();
		    				prev_um_left = &(v_um_solid[i_l].second);
	    				}

	    				--i_l;
	    			}

	    			while ((i_r < v_um_solid.size()) && (v_um_solid[i_r].first == (v_um_solid[i_r-1].first + 1))){

	    				if (!v_um_solid[i_r].second.isSameReferenceUnitig(*prev_um_right)) {

		    				pid_right |= v_um_solid[i_r].second.getData()->getPairID();
		    				prev_um_right = &(v_um_solid[i_r].second);
	    				}

	    				++i_r;
	    			}

	    			if (getNumberSharedPairID(pid_left, pid_right, opt.min_cov_vertices) < opt.min_cov_vertices){

	    				for (++i_l; i_l < i_r; ++i_l) v_um_solid[i_l].second.isEmpty = true;

	    				i = i_r;
	    			}
	    		}
    		}
    	}

    	removeEmptyUnitigMap(v_um_solid);
    }*/

    pair<vector<pair<size_t, const_UnitigMap<UnitigData>>>, vector<pair<size_t, const_UnitigMap<UnitigData>>>> p = {move(v_um_solid), move(v_um_weak)};

    return p;
}

void detectSNPs(CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt){

	if (opt.verbose) cout << "Ratatosk::detectSNPs(): Scanning graph for k-mers with substitutions (SNPs)" << endl;

	const size_t k = dbg.getK();

    auto comp_pair = [](const pair<size_t, const_UnitigMap<UnitigData>>& p1, const pair<size_t, const_UnitigMap<UnitigData>>& p2) {

        return (p1.first < p2.first);
    };

	if (opt.nb_threads == 1){

		size_t count_vertices = 0;

	    for (auto& um : dbg){

	    	if (um.getData()->hasSharedPids()) {

				const string seq_ref = um.referenceUnitigToString();

				vector<pair<size_t, UnitigMap<UnitigData>>> v_um = dbg.searchSequence(seq_ref, false, false, false, true, false);

				if (!v_um.empty()){

					UnitigData* ud = um.getData();

					string seq_final = seq_ref;
					string seq_tried = seq_ref;

					unordered_set<Kmer, KmerHash> s_valid_unitigs, s_invalid_unitigs;

					local_graph_traversal lgt_fw, lgt_bw;

					sort(v_um.begin(), v_um.end(), comp_pair);

			    	for (size_t i = 0; i < v_um.size(); ++i){

			    		const pair<size_t, UnitigMap<UnitigData>>& p = v_um[i];

			    		if (!p.second.isSameReferenceUnitig(um)) { // SNP candidate cannot be on the same unitig

			    			const Kmer head = p.second.getUnitigHead();
				    		const string km_snp = p.second.mappedSequenceToString();
				    		const size_t pos_snp_km = cstrMatch(km_snp.c_str(), seq_ref.c_str() + p.first);

				    		if (pos_snp_km < k){

				    			bool nuc_a_f = false, nuc_c_f = false, nuc_g_f = false, nuc_t_f = false;
					    		bool nuc_a_t = false, nuc_c_t = false, nuc_g_t = false, nuc_t_t = false;
					    		bool nuc_a_k = false, nuc_c_k = false, nuc_g_k = false, nuc_t_k = false;

					    		getAmbiguityRev(seq_final[p.first + pos_snp_km], nuc_a_f, nuc_c_f, nuc_g_f, nuc_t_f);
					    		getAmbiguityRev(seq_tried[p.first + pos_snp_km], nuc_a_t, nuc_c_t, nuc_g_t, nuc_t_t);
					    		getAmbiguityRev(km_snp[pos_snp_km], nuc_a_k, nuc_c_k, nuc_g_k, nuc_t_k);

					    		const char cf = getAmbiguity(nuc_a_f || nuc_a_k, nuc_c_f || nuc_c_k, nuc_g_f || nuc_g_k, nuc_t_f || nuc_t_k);
					    		const char ct = getAmbiguity(nuc_a_t || nuc_a_k, nuc_c_t || nuc_c_k, nuc_g_t || nuc_g_k, nuc_t_t || nuc_t_k);

					    		if (seq_tried[p.first + pos_snp_km] != ct) { // We didn't try that SNP candidate before

					    			seq_tried[p.first + pos_snp_km] = ct;

					    			if (s_valid_unitigs.find(head) != s_valid_unitigs.end()) seq_final[p.first + pos_snp_km] = cf; // Unitig was already analyzed before and was valid
					    			else if (s_invalid_unitigs.find(head) == s_invalid_unitigs.end()) { // Unitig was not already analyzed before or it was invalid

					    				if (isValidSNPcandidate(lgt_fw, lgt_bw, um, p.second, opt.min_cov_vertices)) {

					    					seq_final[p.first + pos_snp_km] = cf;

					    					s_valid_unitigs.insert(head);
					    				}
					    				else s_invalid_unitigs.insert(head);
					    			}
					    		}
					    	}
				    	}
				    }

				    for (size_t i = 0; i < seq_final.length(); ++i){

				    	if (!isDNA(seq_final[i])) ud->add_ambiguity_char(i, seq_final[i]);
				    }

				    ud->runOptimizeAmbiguityChar();
				}
			}

			++count_vertices;

			if (opt.verbose && ((count_vertices%1000000)==0)) cout << "Ratatosk::detectSNPs(): Processed " << count_vertices << " vertices." << endl;
		}
	}
	else {

		auto worker_function = [&](CompactedDBG<UnitigData>::iterator it_c, CompactedDBG<UnitigData>::iterator it_e) {

			size_t l_nb_fp_avoided = 0;

			while (it_c != it_e){

				const UnitigMap<UnitigData>& um = *it_c;

				UnitigData* ud = um.getData();

				if (ud->hasSharedPids()) {

					const string seq_ref = um.referenceUnitigToString();

					vector<pair<size_t, UnitigMap<UnitigData>>> v_um = dbg.searchSequence(seq_ref, false, false, false, true, false);

					if (!v_um.empty()){

						unordered_set<Kmer, KmerHash> s_valid_unitigs, s_invalid_unitigs;

						local_graph_traversal lgt_fw, lgt_bw;

						string seq_final = seq_ref;
						string seq_tried = seq_ref;

						sort(v_um.begin(), v_um.end(), comp_pair);

				    	for (size_t i = 0; i < v_um.size(); ++i){

				    		const pair<size_t, UnitigMap<UnitigData>>& p = v_um[i];

				    		if (!p.second.isSameReferenceUnitig(um)) { // SNP candidate cannot be on the same unitig

				    			const Kmer head = p.second.getUnitigHead();
					    		const string km_snp = p.second.mappedSequenceToString();
					    		const size_t pos_snp_km = cstrMatch(km_snp.c_str(), seq_ref.c_str() + p.first);

					    		if (pos_snp_km < k){

					    			bool nuc_a_f = false, nuc_c_f = false, nuc_g_f = false, nuc_t_f = false;
						    		bool nuc_a_t = false, nuc_c_t = false, nuc_g_t = false, nuc_t_t = false;
						    		bool nuc_a_k = false, nuc_c_k = false, nuc_g_k = false, nuc_t_k = false;

						    		getAmbiguityRev(seq_final[p.first + pos_snp_km], nuc_a_f, nuc_c_f, nuc_g_f, nuc_t_f);
						    		getAmbiguityRev(seq_tried[p.first + pos_snp_km], nuc_a_t, nuc_c_t, nuc_g_t, nuc_t_t);
						    		getAmbiguityRev(km_snp[pos_snp_km], nuc_a_k, nuc_c_k, nuc_g_k, nuc_t_k);

						    		const char cf = getAmbiguity(nuc_a_f || nuc_a_k, nuc_c_f || nuc_c_k, nuc_g_f || nuc_g_k, nuc_t_f || nuc_t_k);
						    		const char ct = getAmbiguity(nuc_a_t || nuc_a_k, nuc_c_t || nuc_c_k, nuc_g_t || nuc_g_k, nuc_t_t || nuc_t_k);

						    		if (seq_tried[p.first + pos_snp_km] != ct) { // We didn't try that SNP candidate before

						    			seq_tried[p.first + pos_snp_km] = ct;

						    			if (s_valid_unitigs.find(head) != s_valid_unitigs.end()) seq_final[p.first + pos_snp_km] = cf; // Unitig was already analyzed before and was valid
						    			else if (s_invalid_unitigs.find(head) == s_invalid_unitigs.end()) { // Unitig was not already analyzed before or it was invalid

						    				if (isValidSNPcandidate(lgt_fw, lgt_bw, um, p.second, opt.min_cov_vertices)) {

						    					seq_final[p.first + pos_snp_km] = cf;

						    					s_valid_unitigs.insert(head);
						    				}
						    				else s_invalid_unitigs.insert(head);
						    			}
						    		}
						    	}
					    	}
					    }

					    for (size_t i = 0; i < seq_final.length(); ++i){

					    	if (!isDNA(seq_final[i])) ud->add_ambiguity_char(i, seq_final[i]);
					    }

					    ud->runOptimizeAmbiguityChar();
					}
				}

				++it_c;
			}
		};

		{
			const size_t slice = 1024;

	        bool stop = false;

	        size_t count_before = 0, count_after = 0;

	        vector<thread> workers; // need to keep track of threads so we can join them

	        mutex mutex_graph;

	        CompactedDBG<UnitigData>::iterator it_c = dbg.begin();
	        CompactedDBG<UnitigData>::iterator it_e = dbg.end();

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	            workers.emplace_back(

	                [&, t]{

						CompactedDBG<UnitigData>::iterator l_it_c, l_it_e;

	                    while (true) {

	                        {
	                            unique_lock<mutex> lock(mutex_graph);

	                            if (stop) return;

	                            l_it_c = it_c;

						        size_t i = 0;

						        count_before = count_after;

						        while ((i < slice) && (it_c != it_e)){

						        	++i;
						        	++it_c;
						        }

	                            if (it_c == it_e) stop = true;

	                            count_after += i;
								l_it_e = it_c;

								if (opt.verbose && ((count_before/1000000) != (count_after/1000000))) cout << "Ratatosk::detectSNPs(): Processed " << count_after << " vertices." << endl;
	                        }

	                        worker_function(l_it_c, l_it_e);
	                    }
	                }
	            );
	        }

	        for (auto& t : workers) t.join();
	    }	
	}
}

bool readGraphData(const string& input_filename, CompactedDBG<UnitigData>& dbg, const bool pid_only, const bool verbose){

    ifstream infile;
    istream in(0);

    Kmer head;

    if (verbose) cout << "Ratatosk::readGraphData(): Reading data associated to unitigs" << endl;

    infile.open(input_filename.c_str(), std::ofstream::in | std::ofstream::binary);
    in.rdbuf(infile.rdbuf());

    unordered_map<uint64_t, const PairID*, CustomHashUint64_t> m_sz;

    m_sz.reserve(dbg.size());

    while (in.good()){

    	if (!head.read(in)) break;

    	const UnitigMap<UnitigData> um = dbg.find(head, true);

    	if (!um.isEmpty) {

    		um.getData()->read(in, pid_only);

    		// See if we can switched our global owned PairID to a global shared PairID 
    		{
	    		SharedPairID& spid = um.getData()->getPairID();

	    		//spid.forceRoaringInternal();

	    		const pair<const PairID*, const PairID*> pids = spid.getPairIDs(); // Get pointers to <global, local> PairIDs

	    		if (pids.first != nullptr) { // IF there is a global PairID

	    			const vector<uint32_t> v_id = pids.first->toVector();
	    			const uint64_t h = wyhash(&v_id[0], v_id.size() * sizeof(uint32_t), 0, _wyp); // Compute a hash from the global ids

	    			// Insert <hash, global PairID pointer>
	    			pair<unordered_map<uint64_t, const PairID*, CustomHashUint64_t>::iterator, bool> p = m_sz.insert(pair<uint64_t, const PairID*>(h, pids.first));

	    			if (!p.second) { // If the hash already exist, it means another SharedPairID might have the same Global PairID

	    				const PairID* g_pid = p.first->second;

	    				if (*g_pid == *(pids.first)) spid.setGlobalPairID(g_pid); // If the two global PairID have the same elements, 
	    			}
	    		}
    		}
    	}
    	else {

    		if (verbose) cerr << "Ratatosk::readGraphData(): Cannot associate data to unitig as the matching unitig is not found in the graph. Abort.";

    		infile.close();

    		return false;
    	}
    }

    return true;
}

void writeGraphData(const string& output_filename, const CompactedDBG<UnitigData>& dbg, const bool verbose){

    ofstream outfile;
    ostream out(0);

    if (verbose) cout << "Ratatosk::writeGraphData(): Writing data associated to unitigs" << endl;

    outfile.open(output_filename.c_str(), std::ofstream::out | std::ofstream::binary);
    out.rdbuf(outfile.rdbuf());

    for (const auto& um : dbg){

    	um.getUnitigHead().write(out);
    	um.getData()->write(out);
    }
}

void writeGraphPairID(CompactedDBG<UnitigData>& dbg, const string& out_fn, const Correct_Opt& opt) {

    ofstream outfile;
    ostream out(0);

    outfile.open(out_fn.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
    out.rdbuf(outfile.rdbuf());

    for (auto& um : dbg){

    	UnitigData* ud = um.getData();

    	if ((ud->getKmerCoverage(um) >= opt.max_cov_vertices) && (ud->getPairID().cardinality() >= opt.max_cov_vertices)) {

			um.getUnitigHead().write(out);

			ud->write(out, true);
	    	ud->clear(false, false, false, false, false, true, false, false);
    	}
    }
}

size_t getMaxKmerCoverage(const CompactedDBG<UnitigData>& dbg, const double top_ratio) {

    auto cmpKmerCovReversed = [](const double a, const double b) {

    	return (a > b);
    };

	vector<double> v_km;

	v_km.reserve(dbg.size());

	for (const auto& um : dbg) v_km.push_back(um.getData()->getKmerCoverage(um));

	sort(v_km.begin(), v_km.end(), cmpKmerCovReversed); // Sort unitigs by coverage

	return v_km[(v_km.size() * top_ratio)];
}

unordered_map<Kmer, vector<streampos>, KmerHash> mergeDiskPairIDs(const string& fn) {

	unordered_map<Kmer, vector<streampos>, KmerHash> umap_pos;

	Kmer head;

    ifstream infile;
    istream in(0);

    infile.open(fn.c_str(), std::ifstream::in | std::ifstream::binary);
    in.rdbuf(infile.rdbuf());

    while (in.good()){

  		SharedPairID l_spid;

		if (!head.read(in)) break;

		umap_pos.insert({head, vector<streampos>()}).first->second.push_back(in.tellg());

		if (in.good()) l_spid.read(in);
	}

    return umap_pos;	
}

pair<string, string> phasing(const CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt, const string& s_raw, const string& s_corr, const string& q_corr) {

	const size_t k = dbg.getK();

	const char q_min = getQual(0.0);
	const char q_max = getQual(1.0);

	const double t_bits_sim = 0.85;

	const size_t max_limit_nb_pids = 1000;
	const size_t nb_bits_elem_tbf = 14; // 0.1% fp rate

	size_t max_nb_pids = 0;

	string s_out, q_out;

	PairID pos2rm, pos_valid_tbf, pos_invalid_tbf;

	vector<pair<size_t, const_UnitigMap<UnitigData>>> v_um;

	// Map read to graph
	{
		const char* str = s_corr.c_str();
		const size_t len = s_corr.length();

        KmerHashIterator<RepHash> it_kmer_h(str, len, k), it_kmer_h_end;

        for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

            const std::pair<uint64_t, int> p = *it_kmer_h; // <k-mer hash, k-mer position in sequence>
            const const_UnitigMap<UnitigData> um = dbg.findUnitig(str, p.second, len);

            if (!um.isEmpty){

            	const UnitigData* ud = um.getData();
            	const size_t card = ud->getPairID().cardinality();

            	if (!ud->isBranching() && (card <= max_limit_nb_pids)) {

            		v_um.push_back({p.second, um});

            		max_nb_pids = max(max_nb_pids, card);
            	}

            	it_kmer_h += um.len - 1;
            }
        }
	}

	{
		const size_t v_um_sz = v_um.size();

		vector<TinyBloomFilter<size_t>> v_tbf;

		v_tbf.reserve(v_um_sz);

		for (size_t i = 0; i < v_um_sz; ++i) {

			const SharedPairID& spid = v_um[i].second.getData()->getPairID();

			TinyBloomFilter<size_t> tbf(max_nb_pids, nb_bits_elem_tbf);

			for (const size_t id : spid) tbf.insert(id);

			v_tbf.push_back(move(tbf));
		}

		for (size_t i = 0; i < v_um_sz; ++i) {

			if (!pos_valid_tbf.contains(i)) {

				bool found = false;
				bool compatible = false;

				const TinyBloomFilter<size_t>& tbf_i = v_tbf[i];

				const size_t pos_i = v_um[i].first;
				const size_t nb_bits_i = tbf_i.cardinality_bits();

				for (size_t j = 0; j < v_um_sz; ++j) {

					if (!pos_invalid_tbf.contains(j)) {

						const TinyBloomFilter<size_t>& tbf_j = v_tbf[j];

						const size_t pos_j = v_um[j].first;
						const size_t min_pos_j = (pos_j < opt.insert_sz) ? 0 : (pos_j - opt.insert_sz);

						if ((pos_i < min_pos_j) || (pos_i > pos_j + opt.insert_sz)) {

							const size_t nb_bits_j = tbf_j.cardinality_bits();
							const size_t nb_shared_bits = tbf_i.and_cardinality_bits(tbf_j);

							compatible = true;

							if ((nb_shared_bits >= t_bits_sim * nb_bits_i) && (nb_shared_bits >= t_bits_sim * nb_bits_j)) {

								found = true;

								pos_valid_tbf.add(i);
								pos_valid_tbf.add(j);

								break;
							}
						}
					}
				}

				if (!found && compatible) {

					const size_t len_i = v_um[i].second.len;

					for (size_t j = pos_i; j < (pos_i + len_i + k); ++j) pos2rm.add(j);

					pos_invalid_tbf.add(i);
				}
			}
		}

		v_tbf.clear();
	}

    // Align corrected reads to uncorrected read
    {
		EdlibAlignConfig config = edlibNewAlignConfig(-1, EDLIB_MODE_NW, EDLIB_TASK_PATH, edlib_iupac_alpha, sz_edlib_iupac_alpha);
		EdlibAlignResult align = edlibAlign(s_raw.c_str(), s_raw.length(), s_corr.c_str(), s_corr.length(), config);

		char* cigar = edlibAlignmentToCigar(align.alignment, align.alignmentLength, EDLIB_CIGAR_STANDARD);
		size_t cigar_len = strlen(cigar);

		size_t cigar_pos = 0;
		size_t prev_cigar_pos = 0;
		size_t target_pos = 0;
		size_t query_pos = 0;

		PairID new_base_pos;

		while (cigar_pos != cigar_len) {

			if ((cigar[cigar_pos] < 0x30) || (cigar[cigar_pos] > 0x39)){ // If current char. is not a number

				const size_t cigar_l = atoi(&cigar[prev_cigar_pos]);

				if (cigar[cigar_pos] == 'M') { // match pr mismatch

					for (size_t i = target_pos; i < target_pos + cigar_l; ++i) {

						if (pos2rm.contains(i)) {

							if (s_corr[i] == s_raw[query_pos + i - target_pos]) q_out += q_corr[i];
							else {

								q_out += q_min;
								new_base_pos.add(s_out.length());
							}

							s_out += s_raw[query_pos + i - target_pos];
						}
						else {

							s_out += s_corr[i];
							q_out += q_corr[i];
						}
					}

					query_pos += cigar_l;
					target_pos += cigar_l;
				}
				else if ((cigar[cigar_pos] == 'I') || (cigar[cigar_pos] == 'S')) {

					if (pos2rm.contains(target_pos)) {

						for (size_t i = 0, s_out_len = s_out.length(); i < cigar_l; ++i) new_base_pos.add(s_out_len + i);

						s_out += s_raw.substr(query_pos, cigar_l);
						q_out += string(cigar_l, q_min);
					}

					query_pos += cigar_l; //insertion or soft-clipping
				}
				else if (cigar[cigar_pos] == 'D') {

					for (size_t i = target_pos; i < target_pos + cigar_l; ++i) {

						if (!pos2rm.contains(i)) {

							s_out += s_corr[i];
							q_out += q_corr[i];
						}
					}

					target_pos += cigar_l;  //deletion
				}

				prev_cigar_pos = cigar_pos + 1;
			}

			++cigar_pos;
		}

		{
			string s_new = std::string(s_out.length(), 'N');

			for (const size_t pos : new_base_pos) {

				const size_t pos_min = (pos < (k-1)) ? 0 : (pos - k + 1);
				const size_t pos_max = ((pos + k) > s_out.length()) ? s_out.length() : (pos + k);
				const size_t len = pos_max - pos_min;

				s_new.replace(pos_min, len, s_out, pos_min, len);
			}

			v_um = dbg.searchSequence(s_new, true, false, false, false, false);

			for (const auto& pum : v_um) {

				for (size_t j = pum.first; j < (pum.first + k); ++j) {

					if (q_out[j] == q_min) q_out[j] = q_max;
				}
			}
		}

		free(cigar);
		edlibFreeAlignResult(align);
	}

	return {s_out, q_out};
}

void phasing_test(CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt, const vector<string>& v_fn_long_raw, const vector<string>& v_fn_long_corr, const string& fn_long_out) {

	const size_t k = dbg.getK();

	const char q_min = getQual(0.0);
	const char q_max = getQual(1.0);

    ofstream outfile;
    ostream out(0);

    outfile.open(fn_long_out.c_str());
    out.rdbuf(outfile.rdbuf());

	auto cmpPairIDcard = [](const pair<size_t, const_UnitigMap<UnitigData>>& a, const pair<size_t, const_UnitigMap<UnitigData>>& b) {

		return (a.second.getData()->getPairID().cardinality() < b.second.getData()->getPairID().cardinality());
	};

	auto getPhaseInconsistent = [&](const string& s_raw, const string& s_corr) {

		const double t_bits_sim = 0.85;

		const size_t max_limit_nb_pids = 1000;
		const size_t nb_bits_elem_tbf = 14; // 0.1% fp rate 

		size_t max_nb_pids = 0;

		PairID pos_valid_tbf, pos_invalid_tbf;

		vector<pair<size_t, const_UnitigMap<UnitigData>>> v_um, v_um_nobranch, v_um_inconsistent;

		// Map read to graph
		{
			const char* str = s_corr.c_str();
			const size_t len = s_corr.length();

	        KmerHashIterator<RepHash> it_kmer_h(str, len, k), it_kmer_h_end;

	        for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

	            const std::pair<uint64_t, int> p = *it_kmer_h; // <k-mer hash, k-mer position in sequence>
	            const const_UnitigMap<UnitigData> um = dbg.findUnitig(str, p.second, len);

	            if (!um.isEmpty) {

	            	const UnitigData* ud = um.getData();

	            	const size_t card = ud->getPairID().cardinality();

	            	v_um.push_back({p.second, um});

	            	if (!ud->isBranching() && (card <= max_limit_nb_pids)) {

	            		v_um_nobranch.push_back({p.second, um});

						max_nb_pids = max(max_nb_pids, card);
	            	}

	            	it_kmer_h += um.len - 1;
	            }
	        }
		}

		{
			const size_t v_um_nobranch_sz = v_um_nobranch.size();

			vector<TinyBloomFilter<size_t>> v_tbf;

			v_tbf.reserve(v_um_nobranch_sz);

			for (const auto& pum : v_um_nobranch) {

				TinyBloomFilter<size_t> tbf(max_nb_pids, nb_bits_elem_tbf);

				for (const size_t id : pum.second.getData()->getPairID()) tbf.insert(id);

				v_tbf.push_back(move(tbf));
			}

			for (size_t i = 0; i < v_um_nobranch_sz; ++i) {

				if (!pos_valid_tbf.contains(i)) {

					bool found = false;
					bool compatible = false;

					const TinyBloomFilter<size_t>& tbf_i = v_tbf[i];

					const size_t pos_i = v_um_nobranch[i].first;
					const size_t nb_bits_i = tbf_i.cardinality_bits();

					for (size_t j = 0; j < v_um_nobranch_sz; ++j) {

						if (!pos_invalid_tbf.contains(j)) {

							const size_t pos_j = v_um_nobranch[j].first;
							const size_t min_pos_j = (pos_j < opt.insert_sz) ? 0 : (pos_j - opt.insert_sz);

							if ((pos_i < min_pos_j) || (pos_i > pos_j + opt.insert_sz)) {

								const TinyBloomFilter<size_t>& tbf_j = v_tbf[j];

								const size_t nb_bits_j = tbf_j.cardinality_bits();
								const size_t nb_shared_bits = tbf_i.and_cardinality_bits(tbf_j);

								compatible = true;

								if ((nb_shared_bits >= t_bits_sim * nb_bits_i) && (nb_shared_bits >= t_bits_sim * nb_bits_j)) {

									found = true;

									pos_valid_tbf.add(i);
									pos_valid_tbf.add(j);

									break;
								}
							}
						}
					}

					if (!found && compatible) {

						v_um_inconsistent.push_back(v_um_nobranch[i]);
						pos_invalid_tbf.add(i);
					}
				}
			}
		}

		return pair<vector<pair<size_t, const_UnitigMap<UnitigData>>>, vector<pair<size_t, const_UnitigMap<UnitigData>>>>(v_um, v_um_inconsistent);
	};

	auto rephaseCorrected = [&](const string& s_raw, const string& s_corr, const string& q_corr, const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_um_inconsistent) {

		if (v_um_inconsistent.empty()) return pair<string, string>(s_corr, q_corr);
    
		EdlibAlignConfig config = edlibNewAlignConfig(-1, EDLIB_MODE_NW, EDLIB_TASK_PATH, edlib_iupac_alpha, sz_edlib_iupac_alpha);
		EdlibAlignResult align = edlibAlign(s_raw.c_str(), s_raw.length(), s_corr.c_str(), s_corr.length(), config);

		char* cigar = edlibAlignmentToCigar(align.alignment, align.alignmentLength, EDLIB_CIGAR_STANDARD);
		size_t cigar_len = strlen(cigar);

		size_t cigar_pos = 0;
		size_t prev_cigar_pos = 0;
		size_t target_pos = 0;
		size_t query_pos = 0;

		PairID new_base_pos, pos_inconsistent;

		string s_out, q_out;

		for (const auto& pum : v_um_inconsistent) {

			for (size_t i = pum.first; i < (pum.first + k + pum.second.len); ++i) pos_inconsistent.add(i);
		}

		while (cigar_pos != cigar_len) {

			if ((cigar[cigar_pos] < 0x30) || (cigar[cigar_pos] > 0x39)){ // If current char. is not a number

				const size_t cigar_l = atoi(&cigar[prev_cigar_pos]);

				if (cigar[cigar_pos] == 'M') { // match pr mismatch

					for (size_t i = target_pos; i < target_pos + cigar_l; ++i) {

						if (pos_inconsistent.contains(i)) {

							if (s_corr[i] == s_raw[query_pos + i - target_pos]) q_out += q_corr[i];
							else {

								q_out += q_min;
								new_base_pos.add(s_out.length());
							}

							s_out += s_raw[query_pos + i - target_pos];
						}
						else {

							s_out += s_corr[i];
							q_out += q_corr[i];
						}
					}

					query_pos += cigar_l;
					target_pos += cigar_l;
				}
				else if ((cigar[cigar_pos] == 'I') || (cigar[cigar_pos] == 'S')) {

					if (pos_inconsistent.contains(target_pos)) {

						for (size_t i = 0, s_out_len = s_out.length(); i < cigar_l; ++i) new_base_pos.add(s_out_len + i);

						s_out += s_raw.substr(query_pos, cigar_l);
						q_out += string(cigar_l, q_min);
					}

					query_pos += cigar_l; //insertion or soft-clipping
				}
				else if (cigar[cigar_pos] == 'D') {

					for (size_t i = target_pos; i < target_pos + cigar_l; ++i) {

						if (!pos_inconsistent.contains(i)) {

							s_out += s_corr[i];
							q_out += q_corr[i];
						}
					}

					target_pos += cigar_l;  //deletion
				}

				prev_cigar_pos = cigar_pos + 1;
			}

			++cigar_pos;
		}

		{
			string s_new = std::string(s_out.length(), 'N');

			for (const size_t pos : new_base_pos) {

				const size_t pos_min = (pos < (k-1)) ? 0 : (pos - k + 1);
				const size_t pos_max = ((pos + k) > s_out.length()) ? s_out.length() : (pos + k);
				const size_t len = pos_max - pos_min;

				s_new.replace(pos_min, len, s_out, pos_min, len);
			}

			const vector<pair<size_t, UnitigMap<UnitigData>>> v_um = dbg.searchSequence(s_new, true, false, false, false, false);

			for (const auto& pum : v_um) {

				for (size_t j = pum.first; j < (pum.first + k); ++j) {

					if (q_out[j] == q_min) q_out[j] = q_max;
				}
			}
		}

		free(cigar);
		edlibFreeAlignResult(align);

		return pair<string, string>(s_out, q_out);
	};

	{
	    bool stop = false;

	    size_t nb_reads_proc = 0;
	    size_t file_id_raw, file_id_corr = 0;

	    vector<thread> workers; // need to keep track of threads so we can join them

	    SpinLock mutex_file_in;
	    SpinLock mutex_file_out;
	    SpinLock mutex_kht;

	    FileParser fp_raw(v_fn_long_raw);
	    FileParser fp_corr(v_fn_long_corr);

	    KmerHashTable<PairID> kht_pid;

	    ofstream outfile;
	    ostream out(0);

	    outfile.open(fn_long_out.c_str());
	    out.rdbuf(outfile.rdbuf());

	    for (size_t t = 0; t < opt.nb_threads; ++t){

	        workers.emplace_back(

	            [&, t]{

	                vector<string> v_in_read, v_in_name, v_in_qual;
	                vector<string> v_in_read_raw, v_in_name_raw;

	                string in_read_corr, in_read_raw;

	                size_t in_read_len = 0;

	                KmerHashTable<PairID> l_kht_pid;

	                while (true) {

	                    mutex_file_in.acquire();

	                    if (stop){

	                        mutex_file_in.release();
	                        return;
	                    }
	                    else {

	                    	// Read corrected long reads
	                        while (in_read_len < opt.buffer_sz) {

	                            stop = !fp_corr.read(in_read_corr, file_id_corr);

	                            if (!stop){

	                                in_read_len += in_read_corr.length();

	                                v_in_read.push_back(move(in_read_corr));
	                                v_in_name.push_back(string(fp_corr.getNameString()));
	                                v_in_qual.push_back((fp_corr.getQualityScoreString() != nullptr) ? string(fp_corr.getQualityScoreString()) : string());

	                                if (opt.verbose && (++nb_reads_proc % 1000 == 0)) cout << "Ratatosk::phasing(): Processed " << nb_reads_proc << " reads " << endl;
	                            }
	                            else break;
	                        }

	                        // Read raw long reads
	                        while (v_in_read_raw.size() < v_in_read.size()) {

	                            if (fp_raw.read(in_read_raw, file_id_raw)) {

	                                v_in_read_raw.push_back(move(in_read_raw));
	                                v_in_name_raw.push_back(string(fp_raw.getNameString()));
	                            }
	                            else break;
	                        }

	                        // Make sure the reads match (same order)
	                        if (v_in_read_raw.size() != v_in_read.size()) {

	                            cerr << "Ratatosk::phasing(): Corrected read file is not in the same order as input long read file. Abort." << endl;
	                            exit(1);
	                        }
	                        else {

	                            for (size_t i = 0; i < v_in_read.size(); ++i) {

	                                if (v_in_name[i].compare(1, v_in_name[i].length() - 1, v_in_name_raw[i], 1, v_in_name_raw[i].length() - 1) != 0) {

	                                    cerr << "Ratatosk::phasing(): Corrected read file is not in the same order as input long read file. Abort." << endl;
	                                    exit(1);
	                                }
	                            }
	                        }

	                        mutex_file_in.release();

                        	for (size_t i = 0; i < v_in_read.size(); ++i) {

                        		pair<vector<pair<size_t, const_UnitigMap<UnitigData>>>, vector<pair<size_t, const_UnitigMap<UnitigData>>>> v_um = getPhaseInconsistent(v_in_read_raw[i], v_in_read[i]);

                        		// Detect unitigs and pair ids which are phase inconsistent
                        		if (!v_um.first.empty() && !v_um.second.empty()) {

	                        		sort(v_um.first.begin(), v_um.first.end(), cmpPairIDcard);

	                        		PairID pid_min = v_um.first.front().second.getData()->getPairID().toPairID();

	                        		for (size_t j = 1; j < v_um.first.size(); ++j) pid_min &= v_um.first[j].second.getData()->getPairID().toPairID();

	                        		if (pid_min.cardinality() != 0) {

	                        			for (const auto& pum : v_um.second) {

	                        				const Kmer head = pum.second.getUnitigHead();
	                        				const pair<KmerHashTable<PairID>::iterator, bool> p_it_kht = l_kht_pid.insert(head, pid_min);

	                        				if (!p_it_kht.second) *(p_it_kht.first) |= pid_min;
	                        			}
	                        		}
                        		}

                        		// Replace phase inconsistent substring with their uncorrected counterpart
                        		pair<string, string> rephased_read = rephaseCorrected(v_in_read_raw[i], v_in_read[i], v_in_qual[i], v_um.second);

                        		v_in_read[i] = move(rephased_read.first);
                        		v_in_qual[i] = move(rephased_read.second);
                        	}

                        	/*{
                        		mutex_kht.acquire();

                        		KmerHashTable<PairID>::const_iterator it_l_kht = l_kht_pid.begin();

                        		for (; it_l_kht != l_kht_pid.end(); ++it_l_kht) {

                        			const pair<KmerHashTable<PairID>::iterator, bool> p_it_kht = kht_pid.insert(it_l_kht.getKey(), *it_l_kht);

                        			if (!p_it_kht.second) *(p_it_kht.first) |= *it_l_kht;
                        		}

                        		mutex_kht.release();
                        	}*/

                        	{
                        		mutex_file_out.acquire();

                        		for (size_t i = 0; i < v_in_read.size(); ++i) out << '@' << v_in_name[i] << "\n" << v_in_read[i] << "\n" << "+" << "\n" << v_in_qual[i] << "\n";

                        		mutex_file_out.release();
                        	}

	                        // Clear
	                        {
	                            in_read_len = 0;

	                            v_in_read.clear();
	                            v_in_qual.clear();
	                            v_in_name.clear();

	                            v_in_read_raw.clear();
	                            v_in_name_raw.clear();

	                            in_read_corr.clear();
	                            in_read_raw.clear();

	                            l_kht_pid.clear();
	                        }
	                    }
	                }
	            }
	        );
	    }

	    for (auto& t : workers) t.join();

	    fp_raw.close();
		fp_corr.close();

		{
    		KmerHashTable<PairID>::const_iterator it_l_kht = kht_pid.begin();

    		for (; it_l_kht != kht_pid.end(); ++it_l_kht) {

    			const Kmer km = it_l_kht.getKey();
    			const PairID& pid = *it_l_kht;
    			const UnitigMap<UnitigData> um = dbg.find(km, true);

    			if (!um.isEmpty) {

    				UnitigData* ud = um.getData();
    				SharedPairID& spid = ud->getPairID();

    				PairID g_pid, l_pid;

    				const pair<const PairID*, const PairID*> pids_ptr = spid.getPairIDs();

    				if (pids_ptr.first != nullptr) g_pid = *(pids_ptr.first) - pid;
    				if (pids_ptr.second != nullptr) l_pid = *(pids_ptr.second) - pid;

    				g_pid.runOptimize();
    				l_pid.runOptimize();

    				spid.clear();

    				spid.setGlobalPairID(g_pid);
    				spid.setLocalPairID(l_pid);
    			}
    		}
		}
	}
}

void addCoverage(CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt, HapReads& hap_reads, const bool long_read_correct){

	if (dbg.isInvalid() || (dbg.length() == 0)) return;

	vector<string> v_seq_in(opt.filename_seq_in);

	const size_t k = dbg.getK();
	const size_t nb_pairs = countRecords(v_seq_in, !long_read_correct, opt.h_seed) + 1;

    const size_t thread_seq_buf_sz = opt.buffer_sz;
    const size_t thread_name_buf_sz = (thread_seq_buf_sz / (k + 1)) + 1;

    const char c_min_confidence_2nd_pass = getQual(opt.min_confidence_2nd_pass, opt.out_qual);

	const uint64_t off_limit_id = 0xffffffffffffffffULL;

	size_t nextID = 1;
	size_t nb_reads = 0;

	uint64_t last_id = 0;

	bool is_unallocated = true;

    double sampling_rate = 0.0;

    size_t len_read;
    size_t pos_read;

    string s, q;

    unordered_map<Kmer, vector<streampos>, KmerHash> umap_pids;

	vector<pair<uint64_t, uint64_t>> read_map_ids_sr(nb_pairs, pair<uint64_t, uint64_t>(off_limit_id, 0x0ULL));

    unordered_map<uint64_t, uint64_t, CustomHashUint64_t> name_hmap;

	FileParser fp(v_seq_in);

	LockGraph lck_g(opt.nb_threads * 1024);
	LockGraph lck_h(opt.nb_threads * 1024);

	string fn_pids_1 = opt.filename_long_out + ".pids.1";
	string fn_pids_2 = opt.filename_long_out + ".pids.2";

	remove(fn_pids_1.c_str());
	remove(fn_pids_2.c_str());

	name_hmap.reserve(nb_pairs);

    auto cmpPair = [](const pair<uint64_t, uint64_t>& a, const pair<uint64_t, uint64_t>& b) {

    	return ((a.second < b.second) || ((a.second == b.second) && (a.first < b.first)));
    };

    auto cmpKmer = [](const pair<Kmer, uint64_t>& a, const pair<Kmer, uint64_t>& b) {

    	return (a.second < b.second);
    };

    auto cmpKmerCovReversed = [](const pair<Kmer, uint64_t>& a, const pair<Kmer, uint64_t>& b) {

    	return (a.second > b.second);
    };

    auto cmpUnitigCovReversed = [](const UnitigMap<UnitigData>& a, const UnitigMap<UnitigData>& b) {

    	return (a.getData()->getKmerCoverage(a) > b.getData()->getKmerCoverage(b));
    };

    auto worker_function1 = [&](char* seq_buf, const size_t seq_buf_sz, const uint64_t* id_buf) {

    	size_t it_id_buf = 0;

        const char* str_end = seq_buf + seq_buf_sz;

        while (seq_buf < str_end) { // for each input

	        const int len = strlen(seq_buf);
	        const uint64_t id = id_buf[it_id_buf];

	        toUpperCase(seq_buf, len);

            KmerHashIterator<RepHash> it_kmer_h(seq_buf, len, k), it_kmer_h_end;
            UnitigMap<UnitigData> um_canonical;

            unordered_set<Kmer, KmerHash> s_km;

            size_t len_centroid = 0;

            for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

                const std::pair<uint64_t, int> p_ = *it_kmer_h; // <k-mer hash, k-mer position in sequence>
	            const UnitigMap<UnitigData> um = dbg.findUnitig(seq_buf, p_.second, len);

	            if (!um.isEmpty) { // Read maps to a Unitig

	            	const uint64_t h = um.getUnitigHead().hash();
	            	
	            	UnitigData* ud = um.getData();

	            	lck_g.lock_unitig(h);

					ud->increaseUnphasedCoverage(um.len); // No haplotype considered for now, we're interested purely in kmer coverage per unitig

	            	const size_t l_nb_reads_centroid = ud->getPairID().size();

	            	lck_g.unlock_unitig(h);

	            	if (um_canonical.isEmpty || ((l_nb_reads_centroid != 0) && (um.size > len_centroid)) || ((len_centroid == 0) && (um.size > um_canonical.size))) {

	            		um_canonical = um;

	            		if ((l_nb_reads_centroid != 0) && (um.size > len_centroid)) len_centroid = um.size;
	            	}

	            	if (!long_read_correct) s_km.insert(um.getUnitigHead().rep());

	            	it_kmer_h += um.len - 1;
	            }
	        }

	        if (!um_canonical.isEmpty){

            	const uint64_t h = um_canonical.getUnitigHead().hash();

            	lck_g.lock_unitig(h);

            	um_canonical.getData()->getPairID().add(id);

            	lck_g.unlock_unitig(h);
            }

            if (!long_read_correct) {

	        	lck_h.lock_unitig(id);

	        	for (const auto& km : s_km) read_map_ids_sr[id].second += km.hash();

	        	lck_h.unlock_unitig(id);
        	}

            seq_buf += len + 1;
            ++it_id_buf;
        }
    };

    auto worker_function2 = [&](char* seq_buf, const size_t seq_buf_sz, const uint64_t* id_buf) {

	    size_t it_id_buf = 0;

        const char* str_end = seq_buf + seq_buf_sz;

        while (seq_buf < str_end) { // for each input

	        const unordered_map<uint64_t, uint64_t, CustomHashUint64_t>::const_iterator it_name_hmap = name_hmap.find(id_buf[it_id_buf]);

	    	const uint64_t read_name_id = (it_name_hmap == name_hmap.end()) ? 0 : it_name_hmap->second;

	        const size_t len = strlen(seq_buf);

            if ((read_name_id != 0) && (read_name_id != off_limit_id)) { // Read was selected by subsampling, is not duplicate nor invalid

	        	const unordered_map<uint64_t, uint64_t, CustomHashUint64_t>::const_iterator it_read2hap = hap_reads.read2hap.find(id_buf[it_id_buf]);

		    	const uint64_t hap_name_id = (it_read2hap == hap_reads.read2hap.end()) ? off_limit_id : it_read2hap->second;

		    	toUpperCase(seq_buf, len);

	            KmerHashIterator<RepHash> it_kmer_h(seq_buf, len, k), it_kmer_h_end;

	            {
		            lck_h.lock_unitig(hap_name_id + 1);

		            if (hap_name_id == off_limit_id) hap_reads.hap2unphasedReads.add(read_name_id - 1);
		            else hap_reads.hap2phasedReads[hap_name_id].add(read_name_id - 1);

		            lck_h.unlock_unitig(hap_name_id + 1);
	        	}

	            for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

	                const std::pair<uint64_t, int> p_ = *it_kmer_h; // <k-mer hash, k-mer position in sequence>
		            const UnitigMap<UnitigData> um = dbg.findUnitig(seq_buf, p_.second, len);

		            if (!um.isEmpty){

		            	const uint64_t h = um.getUnitigHead().hash();

		            	UnitigData* ud = um.getData();

		            	lck_g.lock_unitig(h);

		            	ud->getPairID().add(read_name_id - 1);

		            	if (hap_name_id == off_limit_id) ud->increaseUnphasedCoverage(um.len);
		            	else {

		            		ud->increasePhasedCoverage(um.len);
		            		ud->get_hapID().add(hap_name_id);
		            	}

		            	lck_g.unlock_unitig(h);

		            	it_kmer_h += um.len - 1;
		            }
		        }
	    	}

            seq_buf += len + 1;
            ++it_id_buf;
        }
    };

    auto reading_function1 = [&](char* seq_buf, size_t& seq_buf_sz, uint64_t* name_hash_buf) {

        size_t file_id = 0;
        size_t it_name_hash_buf = 0;

        const size_t sz_seq_buf = thread_seq_buf_sz - k;

        const char* s_str = s.c_str();

        seq_buf_sz = 0;

        while (seq_buf_sz < sz_seq_buf){

            const bool new_reading = (pos_read >= len_read);

            if (!new_reading || fp.read(s, file_id)) {

            	pos_read &= static_cast<size_t>(new_reading) - 1;

                len_read = s.length();
                s_str = s.c_str();

                if ((len_read >= k) && (!long_read_correct || (len_read >= opt.min_len_2nd_pass))) {

                	if (pos_read == 0){ // New read

		            	const uint64_t h_name = wyhash(fp.getNameString(), strlen(fp.getNameString()), opt.h_seed, _wyp);
			        	const pair<unordered_map<uint64_t, uint64_t, CustomHashUint64_t>::const_iterator, bool> p = name_hmap.insert({h_name, nextID});

			        	last_id = p.second ? nextID : p.first->second;
			        	nextID += static_cast<size_t>(p.second);

			        	if (long_read_correct) {

			        		const char* qual = fp.getQualityScoreString();

			        		for (size_t j = 0; j != len_read; ++j) {

			        			if (qual[j] < c_min_confidence_2nd_pass) s[j] = 'N';
			        		}
			        	}
                	}
                	
                	name_hash_buf[it_name_hash_buf++] = last_id;

                    if ((thread_seq_buf_sz - seq_buf_sz - 1) < (len_read - pos_read)){

                        strncpy(&seq_buf[seq_buf_sz], &s_str[pos_read], thread_seq_buf_sz - seq_buf_sz - 1);

                        seq_buf[thread_seq_buf_sz - 1] = '\0';

                        pos_read += sz_seq_buf - seq_buf_sz;
                        seq_buf_sz = thread_seq_buf_sz;

                        break;
                    }
                    else {

                        strcpy(&seq_buf[seq_buf_sz], &s_str[pos_read]);

                        seq_buf_sz += (len_read - pos_read) + 1;
                        pos_read = len_read;
                    }
                }
                else pos_read = len_read;
            }
            else return true;
        }

        return false;
    };

    auto reading_function2 = [&](char* seq_buf, size_t& seq_buf_sz, uint64_t* name_hash_buf) {

        size_t file_id = 0;
        size_t it_name_hash_buf = 0;

        const size_t sz_seq_buf = thread_seq_buf_sz - k;

        const char* s_str = s.c_str();

        seq_buf_sz = 0;

        while (seq_buf_sz < sz_seq_buf){

            const bool new_reading = (pos_read >= len_read);

            if (!new_reading || fp.read(s, file_id)) {

            	pos_read &= static_cast<size_t>(new_reading) - 1;

            	if (opt.verbose && new_reading && ((++nb_reads % 1000000) == 0)) cout << "Ratatosk::addCoverage(): Processed " << nb_reads << " reads." << endl;

                len_read = s.length();
                s_str = s.c_str();

                if ((len_read >= k) && (!long_read_correct || (len_read >= opt.min_len_2nd_pass))) {

		        	if (long_read_correct && new_reading) {

		        		const char* qual = fp.getQualityScoreString();

		        		for (size_t j = 0; j != len_read; ++j) {

		        			if (qual[j] < c_min_confidence_2nd_pass) s[j] = 'N';
		        		}
		        	}

                	name_hash_buf[it_name_hash_buf++] = wyhash(fp.getNameString(), strlen(fp.getNameString()), opt.h_seed, _wyp);

                    if ((thread_seq_buf_sz - seq_buf_sz - 1) < (len_read - pos_read)){

                        strncpy(&seq_buf[seq_buf_sz], &s_str[pos_read], thread_seq_buf_sz - seq_buf_sz - 1);

                        seq_buf[thread_seq_buf_sz - 1] = '\0';

                        pos_read += sz_seq_buf - seq_buf_sz;
                        seq_buf_sz = thread_seq_buf_sz;

                        break;
                    }
                    else {

                        strcpy(&seq_buf[seq_buf_sz], &s_str[pos_read]);

                        seq_buf_sz += (len_read - pos_read) + 1;
                        pos_read = len_read;
                    }
                }
                else pos_read = len_read;
            }
            else return true;
        }

        return false;
    };

   auto getDiskPairID = [&](istream& in, const const_UnitigMap<UnitigData>& um) {

		const unordered_map<Kmer, vector<streampos>, KmerHash>::const_iterator it_umap_um = umap_pids.find(um.getUnitigHead());

		if (it_umap_um != umap_pids.end()) {

	    	const SharedPairID& spid = um.getData()->getPairID();

	    	const pair<const PairID*, const PairID*> p_pids = spid.getPairIDs();

			SharedPairID spid_out;

			PairID l_pid;

	    	vector<PairID> v_pids;
	    	vector<const PairID*> v_pids_ptr;

	    	v_pids.reserve((it_umap_um->second.size() + 1) * 2);

	    	if (p_pids.first != nullptr) v_pids.push_back(*p_pids.first);
	    	if (p_pids.second != nullptr) v_pids.push_back(*p_pids.second);

			for (const auto fpos : it_umap_um->second) {

				SharedPairID spid_tmp;

				in.seekg(fpos);

				spid_tmp.read(in);

				const pair<const PairID*, const PairID*> p_pids_tmp = spid_tmp.getPairIDs();

	    		if (p_pids_tmp.first != nullptr) v_pids.push_back(*p_pids_tmp.first);
	    		if (p_pids_tmp.second != nullptr) v_pids.push_back(*p_pids_tmp.second);
			}

			v_pids_ptr.reserve(v_pids.size());

			for (const auto& pid : v_pids) v_pids_ptr.push_back(&pid);

			l_pid = PairID::fastunion(v_pids_ptr.size(), &(v_pids_ptr.front()));

			spid_out.setLocalPairID(l_pid);

			return spid_out;
		}
		else return um.getData()->getPairID();
    };

    auto getDiskPairIDs = [&](istream& in, const const_UnitigMap<UnitigData>& um) {

		const unordered_map<Kmer, vector<streampos>, KmerHash>::const_iterator it_umap_um = umap_pids.find(um.getUnitigHead());

    	SharedPairID spid = um.getData()->getPairID();

		vector<SharedPairID> v_spids;

		if (it_umap_um != umap_pids.end()) {

			v_spids.reserve(it_umap_um->second.size() + 1);
			v_spids.push_back(move(spid));

			for (const auto fpos : it_umap_um->second) {

				in.seekg(fpos);
				spid.read(in);

				v_spids.push_back(move(spid));
			}
		}
		else v_spids.push_back(move(spid));

		return v_spids;
    };

	auto postProcessUnitigs = [&](istream& in, CompactedDBG<UnitigData>::iterator it_s, CompactedDBG<UnitigData>::iterator it_e) {

		while (it_s != it_e) {

			UnitigData* ud = it_s->getData();

			const SharedPairID pids = getDiskPairID(in, *it_s);

			if (ud->getPhasedKmerCoverage(*it_s) < opt.min_cov_vertices) ud->get_hapID().clear(); // Not enough k-mer coverage for the phased reads -> delete
			else ud->get_hapID().runOptimize();

			ud->setBranching((it_s->getPredecessors().cardinality() > 1) || (it_s->getSuccessors().cardinality() > 1));

			for (const auto& um_succ : it_s->getSuccessors()) {

				const SharedPairID pids_succ = getDiskPairID(in, um_succ);

				if (getNumberSharedPairID(pids, pids_succ, opt.min_cov_vertices) >= opt.min_cov_vertices) ud->setSharedPids(it_s->strand, um_succ.getMappedHead().toString()[k-1]);
			}

			{
				UnitigMap<UnitigData> um_rev(*it_s);

				um_rev.strand = !it_s->strand;

				for (const auto& um_succ : um_rev.getSuccessors()) {

					const SharedPairID pids_succ = getDiskPairID(in, um_succ);

					if (getNumberSharedPairID(pids, pids_succ, opt.min_cov_vertices) >= opt.min_cov_vertices) ud->setSharedPids(um_rev.strand, um_succ.getMappedHead().toString()[k-1]);
				}
			}

			++it_s;
		}
	};

    {
    	if (opt.verbose) cout << "Ratatosk::addCoverage(): Anchoring reads on graph." << endl;

        char** buffer_seq = new char*[opt.nb_threads];
        size_t* buffer_seq_sz = new size_t[opt.nb_threads];
        uint64_t** buffer_name = new uint64_t*[opt.nb_threads];

	    len_read = 0;
	    pos_read = 0;
	    nextID = 1;

	    if (opt.nb_threads == 1){

        	buffer_seq[0] = new char[thread_seq_buf_sz];
        	buffer_name[0] = new uint64_t[thread_name_buf_sz];

            while (!reading_function1(buffer_seq[0], buffer_seq_sz[0], buffer_name[0])) worker_function1(buffer_seq[0], buffer_seq_sz[0], buffer_name[0]);
	    }
	    else {

        	bool eof_fastx = false;

	        vector<thread> workers; // need to keep track of threads so we can join them

	        mutex mutex_file;

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	        	buffer_seq[t] = new char[thread_seq_buf_sz];
	        	buffer_name[t] = new uint64_t[thread_name_buf_sz];

	            workers.emplace_back(

	                [&, t]{

	                    while (true) {

	                        {
	                            unique_lock<mutex> lock(mutex_file);

	                            if (eof_fastx) return;

	                            eof_fastx = reading_function1(buffer_seq[t], buffer_seq_sz[t], buffer_name[t]);
	                        }

	                        worker_function1(buffer_seq[t], buffer_seq_sz[t], buffer_name[t]);
	                    }
	                }
	            );
	        }

	        for (auto& t : workers) t.join();
	    }

        for (size_t t = 0; t < opt.nb_threads; ++t){

        	delete[] buffer_seq[t];
        	delete[] buffer_name[t];
        }

        delete[] buffer_seq;
        delete[] buffer_name;
        delete[] buffer_seq_sz;

	    fp.close();

	    if (opt.verbose) cout << "Ratatosk::addCoverage(): Detecting and removing duplicated reads." << endl;

		nextID = 1;

	    std::random_device rd; //Seed
	    std::default_random_engine generator(rd()); //Random number generator
	    std::uniform_real_distribution<> distribution(0.0, 1.0); //Distribution on which to apply the generator

	    for (const auto& um : dbg) {

			const SharedPairID& pid = um.getData()->getPairID();

			vector<pair<uint64_t, uint64_t>> vp;

			vp.reserve(pid.size());

			for (const auto id : pid) vp.push_back(pair<uint64_t, uint64_t>(id, read_map_ids_sr[id].second));

			sort(vp.begin(), vp.end(), cmpPair);

			size_t prev_id = nextID;

			for (size_t i = 0; i < vp.size(); ++i){

				pair<uint64_t, uint64_t>& p = read_map_ids_sr[vp[i].first];

				if (long_read_correct) {

					if (p.first == off_limit_id) p.first = ((opt.sampling_rate == 1.0) || (distribution(generator) <= opt.sampling_rate)) ? nextID++ : 0;
				}
				else if (p.first == off_limit_id){

					if ((i == 0) || (vp[i].second != vp[i-1].second)) {

						prev_id = ((opt.sampling_rate == 1.0) || (distribution(generator) <= opt.sampling_rate)) ? nextID++ : 0;
					}
					
					p.first = prev_id;
				}
				else prev_id = p.first;
			}
		}

		for (auto& h : name_hmap) h.second = read_map_ids_sr[h.second].first;

		read_map_ids_sr.clear();

		for (auto& um : dbg) um.getData()->clear(); // Clear all unitig data
	}

    {
    	if (opt.verbose) cout << "Ratatosk::addCoverage(): Coloring graph with reads." << endl;

        char** buffer_seq = new char*[opt.nb_threads];
        size_t* buffer_seq_sz = new size_t[opt.nb_threads];
        uint64_t** buffer_name = new uint64_t*[opt.nb_threads];

    	size_t curr_sz_read = 0;

	    len_read = 0;
	    pos_read = 0;

	    fp = FileParser(v_seq_in);

	    if (opt.nb_threads == 1){

        	buffer_seq[0] = new char[thread_seq_buf_sz];
        	buffer_name[0] = new uint64_t[thread_name_buf_sz];

            while (!reading_function2(buffer_seq[0], buffer_seq_sz[0], buffer_name[0])) {

            	worker_function2(buffer_seq[0], buffer_seq_sz[0], buffer_name[0]);

            	curr_sz_read += buffer_seq_sz[0];

            	if (curr_sz_read >= opt.buffer_sz_read2disk) {

		        	writeGraphPairID(dbg, fn_pids_1, opt);

		        	curr_sz_read = 0;
            	}
            }
	    }
	    else {

        	bool eof_fastx = false;

	        vector<thread> workers; // need to keep track of threads so we can join them

	        mutex mutex_file;

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	        	buffer_seq[t] = new char[thread_seq_buf_sz];
	        	buffer_name[t] = new uint64_t[thread_name_buf_sz];
	        }

	        while (!eof_fastx) {

		        for (size_t t = 0; t < opt.nb_threads; ++t){

		            workers.emplace_back(

		                [&, t]{

		                    while (true) {

		                        {
		                            unique_lock<mutex> lock(mutex_file);

		                            if (eof_fastx || (curr_sz_read >= opt.buffer_sz_read2disk)) return;

		                            eof_fastx = reading_function2(buffer_seq[t], buffer_seq_sz[t], buffer_name[t]);
					            	curr_sz_read += buffer_seq_sz[t];
		                        }

		                        worker_function2(buffer_seq[t], buffer_seq_sz[t], buffer_name[t]);
		                    }
		                }
		            );
		        }

		        for (auto& t : workers) t.join();

		        if (curr_sz_read >= opt.buffer_sz_read2disk) {

		        	writeGraphPairID(dbg, fn_pids_1, opt);

		        	curr_sz_read = 0;

		        	workers.clear();
		        }
	    	}
    	}

        for (size_t t = 0; t < opt.nb_threads; ++t){

        	delete[] buffer_seq[t];
        	delete[] buffer_name[t];
        }

        delete[] buffer_seq;
        delete[] buffer_name;
        delete[] buffer_seq_sz;

        name_hmap.clear();
        fp.close();

    	if (check_files(fn_pids_1, false, false)) umap_pids = mergeDiskPairIDs(fn_pids_1); // Reassemble data from disk
	}

	{
		if (opt.verbose) cout << "Ratatosk::addCoverage(): Collecting branching information." << endl;

	    const size_t slice = 1024;

	    CompactedDBG<UnitigData>::iterator it_s = dbg.begin();
	    CompactedDBG<UnitigData>::iterator it_e = dbg.end();

	    if ((opt.nb_threads == 1) || (dbg.size() < slice)){

	    	ifstream infile;
	    	istream in(0);

	    	if (check_files(fn_pids_1, false, false)) {

		    	infile.open(fn_pids_1.c_str(), std::ifstream::in | std::ifstream::binary);
		    	in.rdbuf(infile.rdbuf());
	    	}

	    	postProcessUnitigs(in, it_s, it_e);
	    }
	    else {

	        vector<thread> workers;

	        mutex mutex_graph;

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	            workers.emplace_back(

	                [&]{

	                	CompactedDBG<UnitigData>::iterator it_s_l;
	                	CompactedDBG<UnitigData>::iterator it_e_l;

				    	ifstream infile;
				    	istream in(0);

				    	if (check_files(fn_pids_1, false, false)) {

					    	infile.open(fn_pids_1.c_str(), std::ifstream::in | std::ifstream::binary);
					    	in.rdbuf(infile.rdbuf());
				    	}

	                	while (true){

	                		{
	                			unique_lock<mutex> lock(mutex_graph);

	                			if (it_s == it_e) return;
	                			else {

	                				it_s_l = it_s;
	                				it_e_l = it_s;

	                				for (size_t i = 0; (i < slice) && (it_e_l != it_e); ++i) ++it_e_l;

	                				it_s = it_e_l;
	                			}
	                		}

	                		postProcessUnitigs(in, it_s_l, it_e_l);
	                	}
	                }
	            );
	        }

	        for (auto& t : workers) t.join();
	    }
	}

	const size_t estimated_hap_cov = estimateHaplotypeCoverage(dbg);

	if (estimated_hap_cov >= 10) {

		if (opt.verbose) cout << "Ratatosk::addCoverage(): Subsampling reads." << endl;

		vector<pair<Kmer, size_t>> v_km;

		const double step = 0.05;
		const double sampling_rate = 5.0 / static_cast<double>(estimated_hap_cov);

		const size_t sz_bitset = ((nextID + 63) / 64) + 1;

		unordered_map<size_t, size_t> m_id_compact;

	    std::random_device rd; // Seed
	    std::default_random_engine generator(rd()); // Random number generator
	    std::uniform_real_distribution<> distribution(0.0, 1.0); // Distribution on which to apply the generator

		uint64_t* all_ids = new uint64_t[sz_bitset]();
		uint64_t* all_subsampled_ids = new uint64_t[sz_bitset](); // Subsampled IDs
		uint64_t* sampled_ids = new uint64_t[sz_bitset](); // Locally sampled IDs

		v_km.reserve(dbg.size());

		for (const auto& um : dbg) v_km.push_back(pair<Kmer, size_t>(um.getUnitigHead(), um.getData()->getKmerCoverage(um)));

		sort(v_km.begin(), v_km.end(), cmpKmerCovReversed); // Sort unitigs by coverage

		size_t prev_max_km_cov = 0;
		size_t prev_min_km_cov = 0;

    	ifstream infile;
    	istream in(0);

    	if (check_files(fn_pids_1, false, false)) {

	    	infile.open(fn_pids_1.c_str(), std::ifstream::in | std::ifstream::binary);
	    	in.rdbuf(infile.rdbuf());
    	}

		// Make sure every unitig is covered by at least 2 reads
		{
			for (const auto& um : dbg) {

				const UnitigData* ud = um.getData();

				if (!ud->isBranching()) {

					const SharedPairID spid = getDiskPairID(in, um);
					const PairID pid = subsample(spid, opt.min_cov_vertices);

					for (const uint32_t id : pid) all_subsampled_ids[id >> 6] |= (0x1ULL << (id & 0x3fULL));
				}
			}
		}

		for (double i = 1.0; i > 0.0; i -= step) {

			const size_t pos_min = v_km.size() * i - static_cast<size_t>((v_km.size() * i) == v_km.size());
			const size_t pos_max = v_km.size() * (i - step);

			const size_t min_km_cov = v_km[pos_min].second;
			const size_t max_km_cov = v_km[pos_max].second;

			const size_t slice = 1024;

			in.seekg(0, ios::beg); // Put back cursor at the beginning of file

			if ((max_km_cov != min_km_cov) && (max_km_cov != prev_max_km_cov)) {

				memset(sampled_ids, 0, sz_bitset * sizeof(uint64_t));

				if (opt.nb_threads == 1) {
				
					for (const auto& um : dbg) {

						const size_t um_km_cov = um.getData()->getKmerCoverage(um);

						if ((um_km_cov >= min_km_cov) && (um_km_cov < max_km_cov)) {

							const SharedPairID& spid = um.getData()->getPairID();

							for (const uint32_t id : spid) sampled_ids[id >> 6] |= (0x1ULL << (id & 0x3fULL));
						}
					}

					while (in.good()) {

            			Kmer km;
            			SharedPairID spid;

            			if (!km.read(in)) break;
            			
            			spid.read(in);

        				const const_UnitigMap<UnitigData> um = dbg.find(km);

        				if (!um.isEmpty) {

							const size_t um_km_cov = um.getData()->getKmerCoverage(um);

							if ((um_km_cov >= min_km_cov) && (um_km_cov < max_km_cov)) {

								for (const uint32_t id : spid) sampled_ids[id >> 6] |= (0x1ULL << (id & 0x3fULL));
							}
        				}
					}
				}
				else {

			        vector<thread> workers;

			        mutex mtx_g;

			        CompactedDBG<UnitigData>::iterator it_s = dbg.begin();
			        CompactedDBG<UnitigData>::iterator it_e = dbg.end();

			        uint64_t** l_sampled_ids = new uint64_t*[opt.nb_threads];

			        for (size_t t = 0; t < opt.nb_threads; ++t) l_sampled_ids[t] = new uint64_t[sz_bitset]();

			        // Extract all read ids for unitigs with given range of coverage for read id stored on disk
			        // It is more efficient to read ids from disk sequentially, then query for coverage afterwards
			        {
				        for (size_t t = 0; t < opt.nb_threads; ++t){

				            workers.emplace_back(

				                [&, t]{

			                		while (true) {

			                			vector<pair<Kmer, SharedPairID>> v_km_spid;

			                			v_km_spid.reserve(slice);

				                		{
				                			Kmer km;
				                			SharedPairID spid;

				                			unique_lock<mutex> lock(mtx_g);

				                			if (!in.good()) return;

				                			for (size_t i = 0; in.good() && (i < slice); ++i){

				                				if (!km.read(in)) break;

				                				spid.read(in);

				                				v_km_spid.push_back(pair<Kmer, SharedPairID>(move(km), move(spid)));
				                			}
				                		}

			                			for (const auto& p_km_spid : v_km_spid) {

			                				const const_UnitigMap<UnitigData> um = dbg.find(p_km_spid.first);

			                				if (!um.isEmpty) {

												const size_t um_km_cov = um.getData()->getKmerCoverage(um);

												if ((um_km_cov >= min_km_cov) && (um_km_cov < max_km_cov)) {

													for (const uint32_t id : p_km_spid.second) l_sampled_ids[t][id >> 6] |= (0x1ULL << (id & 0x3fULL));
												}
			                				}
			                			}
		                			}
			                	}
				            );
				        }

				        for (auto& t : workers) t.join();
					}

				    workers.clear();

					// Extract all read ids for unitigs with given range of coverage for read id stored in RAM
					{
				        for (size_t t = 0; t < opt.nb_threads; ++t){

				            workers.emplace_back(

				                [&, t]{

				                	CompactedDBG<UnitigData>::iterator it_s_l;
				                	CompactedDBG<UnitigData>::iterator it_e_l;

							    	while (true) {

				                		{
				                			unique_lock<mutex> lock(mtx_g);

				                			if (it_s == it_e) return;
				                			else {

				                				it_s_l = it_s;
				                				it_e_l = it_s;

				                				for (size_t j = 0; (j < slice) && (it_e_l != it_e); ++j) ++it_e_l;

				                				it_s = it_e_l;
				                			}
				                		}

				                		while (it_s_l != it_e_l) {

											const size_t um_km_cov = it_s_l->getData()->getKmerCoverage(*it_s_l);

											if ((um_km_cov >= min_km_cov) && (um_km_cov < max_km_cov)) {

												const SharedPairID& spid = it_s_l->getData()->getPairID();

												for (const uint32_t id : spid) l_sampled_ids[t][id >> 6] |= (0x1ULL << (id & 0x3fULL));
											}

											++it_s_l;
				                		}
			                		}
			                	}
				            );
				        }

				        for (auto& t : workers) t.join();
			    	}

			    	// Merge read ids collected by the different threads
			        for (size_t t = 0; t < opt.nb_threads; ++t){

			        	for (size_t j = 0; j < sz_bitset; ++j) sampled_ids[j] |= l_sampled_ids[t][j];

			        	delete[] l_sampled_ids[t];
			        }

			        delete[] l_sampled_ids;
				}

				if (min_km_cov >= 5) { // No subsampling for very low coverage vertices

					// Compute sampling rate
					if (opt.verbose) cout << "Ratatosk::addCoverage(): Subsampling reads (" << sampling_rate << ") coloring vertices with k-mer coverage [" << min_km_cov << ", " << max_km_cov << "[" << endl;

					for (size_t i = 0; i < sz_bitset; ++i) {

						sampled_ids[i] &= ~all_ids[i]; // Do not select ids that were already selected previously

						if (sampled_ids[i] != 0) {

							for (size_t j = 0; j < 64; ++j) {

								if (static_cast<bool>(sampled_ids[i] & (0x1ULL << j)) && (distribution(generator) <= sampling_rate)) all_subsampled_ids[i] |= (0x1ULL << j);
							}
						}
					}
				}
				else {

					for (size_t i = 0; i < sz_bitset; ++i) all_subsampled_ids[i] |= sampled_ids[i];
				}

				for (size_t i = 0; i < sz_bitset; ++i) all_ids[i] |= sampled_ids[i];
			}

			prev_max_km_cov = max_km_cov;
			prev_min_km_cov = min_km_cov;
		}

		v_km.clear();

		{
			if (opt.verbose) cout << "Ratatosk::addCoverage(): Compacting subsampled reads." << endl;

			nextID = 1;

			for (size_t i = 0; i < sz_bitset; ++i) {

				if (all_subsampled_ids[i] != 0) {

					for (size_t j = 0; j < 64; ++j) {

						if (static_cast<bool>(all_subsampled_ids[i] & (0x1ULL << j))){

							const size_t curr_id = (i << 6) + j;
							const pair<unordered_map<size_t, size_t>::iterator, bool> p_it = m_id_compact.insert(pair<size_t, size_t>(curr_id, 0));

							if (p_it.second) p_it.first->second = nextID++;
						}
					}
				}
			}

			{
				HapReads hap_r_sub = hap_reads;

				for (size_t i = 0; i < hap_reads.hap2phasedReads.size(); ++i) {

					hap_r_sub.hap2phasedReads[i].clear();

					for (const auto id : hap_reads.hap2phasedReads[i]) {

						if (static_cast<bool>(all_subsampled_ids[id >> 6] & (0x1ULL << (id & 0x3fULL)))) {

							const unordered_map<size_t, size_t>::const_iterator it = m_id_compact.find(id);

							if (it != m_id_compact.end()) hap_r_sub.hap2phasedReads[i].add(it->second);
						}
					}

					hap_r_sub.hap2phasedReads[i].runOptimize();
				}

				{
					hap_r_sub.hap2unphasedReads.clear();

					for (const auto id : hap_reads.hap2unphasedReads) {

						if (static_cast<bool>(all_subsampled_ids[id >> 6] & (0x1ULL << (id & 0x3fULL)))) {

							const unordered_map<size_t, size_t>::const_iterator it = m_id_compact.find(id);

							if (it != m_id_compact.end()) hap_r_sub.hap2unphasedReads.add(it->second);
						}
					}

					hap_r_sub.hap2unphasedReads.runOptimize();
				}

				hap_reads = move(hap_r_sub);
			}
		}

		{
			if (opt.verbose) cout << "Ratatosk::addCoverage(): Coloring graph with subsampled reads." << endl;

	    	ifstream infile;
	    	istream in(0);

	    	ofstream outfile;
	    	ostream out(0);

	    	if (check_files(fn_pids_1, false, false)) {

		    	infile.open(fn_pids_1.c_str(), std::ifstream::in | std::ifstream::binary);
		    	in.rdbuf(infile.rdbuf());

			    outfile.open(fn_pids_2.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
			    out.rdbuf(outfile.rdbuf());
	    	}

	    	umap_pids.clear();

			if (opt.nb_threads == 1) {

				for (auto& um : dbg) {

					SharedPairID sub_spids;

					SharedPairID& spids = um.getData()->getPairID();

					for (const uint32_t id : spids) {

						if (static_cast<bool>(all_subsampled_ids[id >> 6] & (0x1ULL << (id & 0x3fULL)))) {

							const unordered_map<size_t, size_t>::const_iterator it = m_id_compact.find(id);

							if (it != m_id_compact.end()) sub_spids.add(it->second);
						}
					}

					sub_spids.runOptimize();

					spids = move(sub_spids);
				}

				while (in.good()) {

        			Kmer km;
        			SharedPairID spid, sub_spids;

        			if (!km.read(in)) break;
        			
        			spid.read(in);

					for (const uint32_t id : spid) {

						if (static_cast<bool>(all_subsampled_ids[id >> 6] & (0x1ULL << (id & 0x3fULL)))) {

							const unordered_map<size_t, size_t>::const_iterator it = m_id_compact.find(id);

							if (it != m_id_compact.end()) sub_spids.add(it->second);
						}
					}

					sub_spids.runOptimize();

					pair<unordered_map<Kmer, vector<streampos>, KmerHash>::iterator, bool> it_umap_um = umap_pids.insert({km, vector<streampos>()});

					km.write(out); // Write to disk position of unitig head k-mer

					it_umap_um.first->second.push_back(out.tellp()); // Insert position of next disk PairID associated with that unitig

    				sub_spids.write(out); // Write to disk the new PairID of that unitig
				}
			}
			else {

				const size_t slice = 1024;

		        vector<thread> workers;

		        mutex mtx_g, mtx_f;

		        {
			        CompactedDBG<UnitigData>::iterator it_s = dbg.begin();
			        CompactedDBG<UnitigData>::iterator it_e = dbg.end();

			        for (size_t t = 0; t < opt.nb_threads; ++t){

			            workers.emplace_back(

			                [&]{

			                	CompactedDBG<UnitigData>::iterator it_s_l;
			                	CompactedDBG<UnitigData>::iterator it_e_l;

						    	while (true) {

			                		{
			                			unique_lock<mutex> lock(mtx_g);

			                			if (it_s == it_e) return;
			                			else {

			                				it_s_l = it_s;
			                				it_e_l = it_s;

			                				for (size_t i = 0; (i < slice) && (it_e_l != it_e); ++i) ++it_e_l;

			                				it_s = it_e_l;
			                			}
			                		}

			                		while (it_s_l != it_e_l) {

										SharedPairID sub_spids;

										SharedPairID& spids = it_s_l->getData()->getPairID();

										for (const uint32_t id : spids) {

											if (static_cast<bool>(all_subsampled_ids[id >> 6] & (0x1ULL << (id & 0x3fULL)))) {

												const unordered_map<size_t, size_t>::const_iterator it = m_id_compact.find(id);

												if (it != m_id_compact.end()) sub_spids.add(it->second);
											}
										}

										sub_spids.runOptimize();

										spids = move(sub_spids);

										++it_s_l;
			                		}
		                		}
			                }
			            );
			        }

			        for (auto& t : workers) t.join();
		    	}

		    	workers.clear();

		        {
			        for (size_t t = 0; t < opt.nb_threads; ++t){

			            workers.emplace_back(

			                [&, t]{

		                		while (true) {

		                			vector<pair<Kmer, SharedPairID>> v_km_spid;

		                			v_km_spid.reserve(slice);

			                		{
			                			Kmer km;
			                			SharedPairID spid;

			                			unique_lock<mutex> lock(mtx_g);

			                			if (!in.good()) return;

			                			for (size_t i = 0; in.good() && (i < slice); ++i){

			                				if (!km.read(in)) break;

			                				spid.read(in);

			                				v_km_spid.push_back(pair<Kmer, SharedPairID>(move(km), move(spid)));
			                			}
			                		}

		                			for (auto& p_km_spid : v_km_spid) {

										SharedPairID sub_spids;

										for (const uint32_t id : p_km_spid.second) {

											if (static_cast<bool>(all_subsampled_ids[id >> 6] & (0x1ULL << (id & 0x3fULL)))) {

												const unordered_map<size_t, size_t>::const_iterator it = m_id_compact.find(id);

												if (it != m_id_compact.end()) sub_spids.add(it->second);
											}
										}

										sub_spids.runOptimize();

										p_km_spid.second = move(sub_spids);
		                			}

		                			{
		                				unique_lock<mutex> lock(mtx_f);

		                				for (const auto& p_km_spid : v_km_spid) {

		                					pair<unordered_map<Kmer, vector<streampos>, KmerHash>::iterator, bool> it_umap_um = umap_pids.insert({p_km_spid.first, vector<streampos>()});

											p_km_spid.first.write(out); // Write to disk position of unitig head k-mer

											it_umap_um.first->second.push_back(out.tellp()); // Insert position of next disk PairID associated with that unitig

						    				p_km_spid.second.write(out); // Write to disk the new PairID of that unitig
		                				}
		                			}
	                			}
		                	}
			            );
			        }

			        for (auto& t : workers) t.join();
		    	}
			}

			infile.close();
			outfile.close();

			remove(fn_pids_1.c_str()); // Remove non-subsampled reads ids from disk
			swap(fn_pids_1, fn_pids_2); // Replace read id file with subsampled reads ids
		}

		delete[] all_ids;
		delete[] all_subsampled_ids;
		delete[] sampled_ids;
	}

	{
		if (opt.verbose) cout << "Ratatosk::addCoverage(): Compacting colors." << endl;

    	ifstream infile;
    	istream in(0);

   		vector<pair<Kmer, size_t>> v_km;

    	if (check_files(fn_pids_1, false, false)) {

	    	infile.open(fn_pids_1.c_str(), std::ifstream::in | std::ifstream::binary);
	    	in.rdbuf(infile.rdbuf());
    	}

		v_km.reserve(dbg.size());

		for (const auto& um : dbg) {

			// Only use branching vertices
			if (um.getData()->isBranching() && (um.getData()->getKmerCoverage(um) >= 3 * estimated_hap_cov)) {

				v_km.push_back(pair<Kmer, size_t>(um.getUnitigHead(), um.getData()->getKmerCoverage(um)));
			}
		}

		sort(v_km.begin(), v_km.end(), cmpKmerCovReversed); // Sort unitigs by coverage

		for (const auto& p_km : v_km) {

			const UnitigMap<UnitigData> um = dbg.find(p_km.first, true);

			if (!um.isEmpty) {

				UnitigData* um_ud = um.getData();

				if (!um_ud->getVisitStatus()) {

					const SharedPairID spids = getDiskPairID(in, um);

					PairID inter = spids.toPairID();

					size_t max_card_inter = spids.cardinality() * opt.min_color_sharing;

					unordered_set<Kmer, KmerHash> s_km_visited, s_km_valid; // To remember which nodes have been visited locally
					queue<UnitigMap<UnitigData>> q;

					s_km_visited.insert(p_km.first.rep());
					q.push(um);

					while (!q.empty()) {

						const UnitigMap<UnitigData> l_um = q.front();

						vector<UnitigMap<UnitigData>> v_um_neigh;

						v_um_neigh.reserve(8); // Only 8 possible neighbors (4 predecessors + 4 successors)
						q.pop();

						for (const auto& l_um_neigh : l_um.getSuccessors()) {

							if (s_km_visited.insert(l_um_neigh.getUnitigHead().rep()).second && !l_um_neigh.getData()->getVisitStatus()) v_um_neigh.push_back(l_um_neigh);
						}

						for (const auto& l_um_neigh : l_um.getPredecessors()) {

							if (s_km_visited.insert(l_um_neigh.getUnitigHead().rep()).second && !l_um_neigh.getData()->getVisitStatus()) v_um_neigh.push_back(l_um_neigh);
						}

						sort(v_um_neigh.begin(), v_um_neigh.end(), cmpUnitigCovReversed); // Sort unitigs by coverage

						for (const auto& l_um_neigh : v_um_neigh) {

							const SharedPairID l_spid_neigh = getDiskPairID(in, l_um_neigh);

							PairID l_inter = inter & l_spid_neigh.toPairID();

							// New intersection shares at least (opt.min_color_sharing * 100)% of its colors with previously "accepted" unitigs
							if ((l_inter.cardinality() >= (l_spid_neigh.cardinality() * opt.min_color_sharing)) && (l_inter.cardinality() >= max_card_inter)) {

								inter = move(l_inter);
								max_card_inter = max(max_card_inter, static_cast<size_t>(l_spid_neigh.cardinality() * opt.min_color_sharing));

								s_km_valid.insert(l_um_neigh.getUnitigHead().rep());
								q.push(l_um_neigh);
							}
						}
					}

					if (!s_km_valid.empty()) {

						SharedPairID new_spid;

						inter.runOptimize();

						new_spid.setGlobalPairID(inter);
						new_spid.setLocalPairID(spids.toPairID());

						new_spid.runOptimize();

						um_ud->getPairID() = move(new_spid);
						um_ud->setVisitStatus(true);

						const pair<const PairID*, const PairID*> p = um.getData()->getPairID().getPairIDs();

						for (const auto& km : s_km_valid) {

							const UnitigMap<UnitigData> um_neigh = dbg.find(km, true);

							if (!um_neigh.isEmpty) {

								const SharedPairID l_spid = getDiskPairID(in, um_neigh);

								SharedPairID l_new_spid;

								l_new_spid.setGlobalPairID(p.first);
								l_new_spid.setLocalPairID(l_spid.toPairID());

								l_new_spid.runOptimize();

								um_neigh.getData()->getPairID() = move(l_new_spid);
								um_neigh.getData()->setVisitStatus(true);

							}
						}
					}
				}
			}
		}

		if (opt.nb_threads == 1) {

			for (const auto& um : dbg){

				UnitigData* ud = um.getData();

				if (!ud->getVisitStatus()) {

					SharedPairID spids = getDiskPairID(in, um);

					ud->getPairID() = move(spids);
					ud->getPairID().runOptimize();
				}
				else ud->setVisitStatus(false);
			}
		}
		else {

			const size_t slice = 1024;

	        vector<thread> workers;

	        mutex mtx_g, mtx_f;

	        CompactedDBG<UnitigData>::iterator it_s = dbg.begin();
	        CompactedDBG<UnitigData>::iterator it_e = dbg.end();

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	            workers.emplace_back(

	                [&]{

	                	CompactedDBG<UnitigData>::iterator it_s_l;
	                	CompactedDBG<UnitigData>::iterator it_e_l;

	                	while (true){

	                		{
	                			unique_lock<mutex> lock(mtx_g);

	                			if (it_s == it_e) return;
	                			else {

	                				it_s_l = it_s;
	                				it_e_l = it_s;

	                				for (size_t i = 0; (i < slice) && (it_e_l != it_e); ++i) ++it_e_l;

	                				it_s = it_e_l;
	                			}
	                		}

	                		while (it_s_l != it_e_l) {

								UnitigData* ud = it_s_l->getData();

								if (!ud->getVisitStatus()) {

									SharedPairID spids;

									{
										unique_lock<mutex> lock(mtx_f);

										spids = getDiskPairID(in, *it_s_l);
									}

									ud->getPairID() = move(spids);
									ud->getPairID().runOptimize();
								}
								else ud->setVisitStatus(false);

								++it_s_l;
	                		}
	                	}
	                }
	            );
	        }

	        for (auto& t : workers) t.join();
		}
	}

	if (!umap_pids.empty() && (remove(fn_pids_1.c_str()) != 0)) cerr << "Ratatosk::addCoverage(): Could not remove tmp file " << fn_pids_1 << endl;
}

void addPhasing(CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt, HapReads& hap_r, const bool long_read_phasing, const bool mapHapReads) {

	const size_t k = dbg.getK();
	const uint64_t off_limit_id = 0xffffffffffffffffULL;

    string s;

	LockGraph lck_g(opt.nb_threads * 1024);

    size_t len_read;
    size_t pos_read;

    size_t nb_reads = 0;

    const size_t thread_seq_buf_sz = opt.buffer_sz;
    const size_t thread_name_buf_sz = (thread_seq_buf_sz / (k + 1)) + 1;

    const vector<string>& v_phase_filenames = long_read_phasing ? opt.filenames_long_phase : opt.filenames_short_phase;

    vector<string> v_read_filenames = long_read_phasing ? opt.filenames_long_in : opt.filename_seq_in;

    if (long_read_phasing) v_read_filenames.insert(v_read_filenames.end(), opt.filenames_helper_long_in.begin(), opt.filenames_helper_long_in.end());

    auto reading_function1 = [&](FileParser& fp, char* seq_buf, size_t& seq_buf_sz, uint64_t* hap_hash_buf) {

        size_t file_id = 0;
        size_t it_name_hash_buf = 0;

        const size_t sz_seq_buf = thread_seq_buf_sz - k;

        const char* s_str = s.c_str();

        seq_buf_sz = 0;

        while (seq_buf_sz < sz_seq_buf){

            const bool new_reading = (pos_read >= len_read);

            if (!new_reading || fp.read(s, file_id)) {

            	pos_read &= static_cast<size_t>(new_reading) - 1;

            	if (opt.verbose && new_reading && ((++nb_reads % 1000000) == 0)) cout << "Ratatosk::addPhasing(): Processed " << nb_reads << " reads." << endl;

                len_read = s.length();
                s_str = s.c_str();

                if (len_read >= k){

                	const uint64_t h_name = wyhash(fp.getNameString(), strlen(fp.getNameString()), opt.h_seed, _wyp);
                	const unordered_map<uint64_t, uint64_t, CustomHashUint64_t>::const_iterator it_read2hap = hap_r.read2hap.find(h_name);

                	if ((it_read2hap != hap_r.read2hap.end()) && (it_read2hap->second != off_limit_id)){

                		hap_hash_buf[it_name_hash_buf++] = it_read2hap->second;

	                    if ((thread_seq_buf_sz - seq_buf_sz - 1) < (len_read - pos_read)){

	                        strncpy(&seq_buf[seq_buf_sz], &s_str[pos_read], thread_seq_buf_sz - seq_buf_sz - 1);

	                        seq_buf[thread_seq_buf_sz - 1] = '\0';

	                        pos_read += sz_seq_buf - seq_buf_sz;
	                        seq_buf_sz = thread_seq_buf_sz;

	                        break;
	                    }
	                    else {

	                        strcpy(&seq_buf[seq_buf_sz], &s_str[pos_read]);

	                        seq_buf_sz += (len_read - pos_read) + 1;
	                        pos_read = len_read;
	                    }
                	}
                	else pos_read = len_read;
                }
                else pos_read = len_read;
            }
            else return true;
        }

        return false;
    };

    auto worker_function1 = [&](char* seq_buf, const size_t seq_buf_sz, const uint64_t* hap_buf) {

    	uint64_t it_min_h;

	    RepHash rep;

	    size_t it_hap_buf = 0;

        const char* str_end = seq_buf + seq_buf_sz;

        while (seq_buf < str_end) { // for each input

	        const int len = strlen(seq_buf);

        	toUpperCase(seq_buf, len);

            KmerHashIterator<RepHash> it_kmer_h(seq_buf, len, k), it_kmer_h_end;

            for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

                const std::pair<uint64_t, int> p_ = *it_kmer_h; // <k-mer hash, k-mer position in sequence>
	            const UnitigMap<UnitigData> um = dbg.findUnitig(seq_buf, p_.second, len);

	            if (!um.isEmpty) { // Read maps to a Unitig

	            	const uint64_t h = um.getUnitigHead().hash();

	            	UnitigData* ud = um.getData();

	            	lck_g.lock_unitig(h);

	            	ud->get_hapID().add(hap_buf[it_hap_buf]);
		            //ud->increasePhasedCoverage(um.len);

	            	lck_g.unlock_unitig(h);

	            	it_kmer_h += um.len - 1;
	            }
	        }

            seq_buf += len + 1;
            ++it_hap_buf;
        }
    };

	auto loadPhasing = [&](const vector<string>& v_filenames_phase) {

		const size_t sz_buffer = 16384;

    	char buffer[sz_buffer];

    	int buffer_occupancy = 0;

    	uint64_t hap2read_sz = 0;

    	if (opt.verbose) cout << "Ratatosk::addPhasing(): Associating " << (long_read_phasing ? "long" : "short") << " reads to their haploblocks" << endl;

		for (const auto& filename : v_filenames_phase){

            gzFile gfp = gzopen(filename.c_str(), "r");

            if (gfp != Z_NULL) {

            	int l_read = gzread(gfp, buffer, sz_buffer);

	            while (l_read != 0){

	            	const char* l_buffer = buffer;
	            	const char* l_next_buffer = nullptr;

	            	size_t l_buffer_occupancy = buffer_occupancy + l_read;

	            	l_next_buffer = static_cast<const char*>(memchr(l_buffer, '\n', l_buffer_occupancy));

	            	while (l_next_buffer != nullptr) {

		            	if (l_buffer[0] != '#'){ // This is a comment or a header

						    const char* haplotype_cstr = strchr(l_buffer, '\t');
						    const char* haploblock_cstr = (haplotype_cstr == nullptr) ? nullptr : strchr(haplotype_cstr + 1, '\t');

						    if (haploblock_cstr != nullptr) {

						    	const string haplotype_str(haplotype_cstr + 1, haploblock_cstr - (haplotype_cstr + 1));
						    	const string haploblock_str(haploblock_cstr + 1, l_next_buffer - (haploblock_cstr + 1));

						    	if ((haploblock_str != "none") && (haplotype_str != "none")) {

							    	const pair<unordered_map<string, uint64_t, CustomHashString>::const_iterator, bool> p_block = hap_r.hapBlock2id.insert({haploblock_str, hap_r.block_id});
							    	const pair<unordered_map<string, uint64_t, CustomHashString>::const_iterator, bool> p_type = hap_r.hapType2id.insert({haplotype_str, hap_r.type_id});

							    	const uint64_t hap_block_type = (p_block.first->second << 1) + p_type.first->second;
							    	const uint64_t h_record = wyhash(l_buffer, haplotype_cstr - l_buffer, opt.h_seed, _wyp);

							    	hap_r.block_id += static_cast<size_t>(p_block.second);
							    	hap_r.type_id += static_cast<size_t>(p_type.second);

							    	hap2read_sz = max(hap2read_sz, hap_block_type);

					        		pair<unordered_map<uint64_t, uint64_t, CustomHashUint64_t>::iterator, bool> p_read2hap = hap_r.read2hap.insert({h_record, hap_block_type});

					        		// Insertion did not take place:
					        		// - Long read phasing: ID collision -> Set read ID to unphased
					        		// - Short read phasing: Collision is due to mates from same pair. If Haploblock is incompatible, set read ID to unphased
					        		if (!p_read2hap.second && (long_read_phasing || (!long_read_phasing && (p_read2hap.first->second != hap_block_type)))) p_read2hap.first->second = off_limit_id;
				        		}
						    }
							else cerr << "Ratatosk::addPhasing(): Malformed phasing record. Record is discarded." << endl;
						}

						l_buffer_occupancy -= (l_next_buffer - l_buffer) + 1;
						l_buffer = l_next_buffer + 1;
						l_next_buffer = static_cast<const char*>(memchr(l_buffer, '\n', l_buffer_occupancy));
					}

					if (l_buffer_occupancy != 0) memmove(buffer, l_buffer, l_buffer_occupancy);
					
					buffer_occupancy = l_buffer_occupancy;
					l_read = gzread(gfp, (buffer + buffer_occupancy), (sz_buffer - buffer_occupancy));
	            }

	            gzclose(gfp);

	            hap_r.hap2phasedReads = vector<PairID>(hap2read_sz + 1);
        	}
        	else cerr << "Ratatosk::addPhasing(): Could not open file " << filename << " for phasing. File is discarded." << endl;
    	}

    	if (opt.verbose) cout << "Ratatosk::addPhasing(): " << hap_r.read2hap.size() << " phased reads added." << endl;
    	if (opt.verbose) cout << "Ratatosk::addPhasing(): " << hap_r.hapBlock2id.size() << " haploblocks with " << hap_r.hapType2id.size() << " haplotypes compacted." << endl;
	};

    auto runHapMapping = [&](const vector<string>& filenames) {

    	if (opt.verbose) cout << "Ratatosk::addPhasing(): Associating haploblocks to unitigs." << endl;

        char** buffer_seq = new char*[opt.nb_threads];
        size_t* buffer_seq_sz = new size_t[opt.nb_threads];
        uint64_t** buffer_name = new uint64_t*[opt.nb_threads];

        FileParser fp(filenames);

	    len_read = 0;
	    pos_read = 0;

	    if (opt.nb_threads == 1){

        	buffer_seq[0] = new char[thread_seq_buf_sz];
        	buffer_name[0] = new uint64_t[thread_name_buf_sz];

            while (!reading_function1(fp, buffer_seq[0], buffer_seq_sz[0], buffer_name[0])) worker_function1(buffer_seq[0], buffer_seq_sz[0], buffer_name[0]);
	    }
	    else {

        	bool stop = false;

	        vector<thread> workers; // need to keep track of threads so we can join them

	        mutex mutex_file;

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	        	buffer_seq[t] = new char[thread_seq_buf_sz];
	        	buffer_name[t] = new uint64_t[thread_name_buf_sz];

	            workers.emplace_back(

	                [&, t]{

	                    while (true) {

	                        {
	                            unique_lock<mutex> lock(mutex_file);

	                            if (stop) return;

	                            stop = reading_function1(fp, buffer_seq[t], buffer_seq_sz[t], buffer_name[t]);
	                        }

	                        worker_function1(buffer_seq[t], buffer_seq_sz[t], buffer_name[t]);
	                    }
	                }
	            );
	        }

	        for (auto& t : workers) t.join();
    	}

    	for (auto& um : dbg){

    		UnitigData* ud = um.getData();
    		PairID& hap_ids = ud->get_hapID();

			/*if (ud->getPhasedKmerCoverage(um) < opt.min_cov_vertices) hap_ids.clear(); // Not enough k-mer coverage for the phased reads -> delete
			else*/ hap_ids.runOptimize();

			//ud->resetCoverage();
		}

        for (size_t t = 0; t < opt.nb_threads; ++t){

        	delete[] buffer_seq[t];
        	delete[] buffer_name[t];
        }

        delete[] buffer_seq;
        delete[] buffer_name;
        delete[] buffer_seq_sz;

        fp.close();
	};

	if (!v_phase_filenames.empty() && !v_read_filenames.empty()){

		loadPhasing(v_phase_filenames);

		if (mapHapReads) runHapMapping(v_read_filenames);
	}
}

pair<BlockedBloomFilter, unordered_set<uint64_t, CustomHashUint64_t>> buildBBF(const vector<string>& v_filenames_in, const Correct_Opt& opt, const size_t k, const size_t g, const bool long_reads) {

	size_t nb_unique_kmers = 0;
	size_t nb_non_unique_kmers = 0;

	// Estimate parameters of the Blocked Bloom Filter
	{
	    KmerStream_Build_opt kms_opt;

	    kms_opt.threads = opt.nb_threads;
	    kms_opt.verbose = opt.verbose;
	    kms_opt.files = v_filenames_in;
	    kms_opt.k = k;
	    kms_opt.g = g;
	    kms_opt.q = 0;

	    KmerStream kms(kms_opt);

	    nb_unique_kmers = max(1UL, kms.KmerF0());
	    nb_non_unique_kmers = max(1UL, nb_unique_kmers - min(nb_unique_kmers, kms.Kmerf1()));

	    if (opt.verbose) cout << "Estimated " << nb_unique_kmers << " unique k-mers and " << nb_non_unique_kmers << " non-unique k-mers." << endl;
	}

	BlockedBloomFilter bf_uniq(nb_unique_kmers, opt.nb_bits_unique_kmers_bf);
	BlockedBloomFilter bf_non_uniq(nb_non_unique_kmers, opt.nb_bits_non_unique_kmers_bf);

	unordered_set<uint64_t, CustomHashUint64_t> name_hset;

	FileParser fp(v_filenames_in);

    string s;

    size_t len_read = 0;
    size_t pos_read = 0;

    const bool multi_threaded = (opt.nb_threads != 1);

    // Main worker thread
    auto worker_function = [&](char* seq_buf, const size_t seq_buf_sz) {

        const char* str_end = seq_buf + seq_buf_sz;

        while (seq_buf < str_end) { // for each input

            const int len = strlen(seq_buf);

            toUpperCase(seq_buf, len);

            KmerHashIterator<RepHash> it_kmer_h(seq_buf, len, k), it_kmer_h_end;
        	minHashIterator<RepHash> it_min(seq_buf, len, k, g, RepHash(), true);

            for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

                const pair<uint64_t, int> p_ = *it_kmer_h; // <k-mer hash, k-mer position in sequence>

                it_min += (p_.second - it_min.getKmerPosition()); //If one or more k-mer were jumped because contained non-ACGT char.

                const uint64_t min_hr = it_min.getHash();

                if (!bf_uniq.insert(p_.first, min_hr, multi_threaded)) bf_non_uniq.insert(p_.first, min_hr, multi_threaded);
            }

            seq_buf += len + 1;
        }
    };

    auto reading_function = [&](char* seq_buf, size_t& seq_buf_sz) {

        size_t file_id = 0;

        const size_t sz_buf = opt.buffer_sz - k;

        const char* s_str = s.c_str();

        seq_buf_sz = 0;

        while (seq_buf_sz < sz_buf) {

            const bool new_reading = (pos_read >= len_read);

            if (!new_reading || fp.read(s, file_id)) {

                pos_read &= static_cast<size_t>(new_reading) - 1;

                len_read = s.length();
                s_str = s.c_str();

                if (len_read >= k){

                    if ((opt.buffer_sz - seq_buf_sz - 1) < (len_read - pos_read)){

                        strncpy(seq_buf + seq_buf_sz, s_str + pos_read, opt.buffer_sz - seq_buf_sz - 1);

                        seq_buf[opt.buffer_sz - 1] = '\0';

                        pos_read += sz_buf - seq_buf_sz;
                        seq_buf_sz = opt.buffer_sz;

                        break;
                    }
                    else {

                        strcpy(seq_buf + seq_buf_sz, s_str + pos_read);

                        seq_buf_sz += (len_read - pos_read) + 1;
                        pos_read = len_read;
                    }
                }
                else pos_read = len_read;
            }
            else return true;
        }

        return false;
    };

    {
        bool stop = false;

        vector<thread> workers; // need to keep track of threads so we can join them

        mutex mutex_file;

        for (size_t t = 0; t < opt.nb_threads; ++t){

            workers.emplace_back(

                [&]{

                    char* buffer_seq = new char[opt.buffer_sz]();

                    size_t buffer_seq_sz = 0;

                    while (true) {

                        {
                            unique_lock<mutex> lock(mutex_file);

                            if (stop) break;

                            stop = reading_function(buffer_seq, buffer_seq_sz);
                        }

                        worker_function(buffer_seq, buffer_seq_sz);
                    }

                    delete[] buffer_seq;
                }
            );
        }

        for (auto& t : workers) t.join();
    }

    fp.close();

	return pair<BlockedBloomFilter, unordered_set<uint64_t, CustomHashUint64_t>>(bf_non_uniq, name_hset);
}

BlockedBloomFilter buildBBF(const CompactedDBG<UnitigData>& dbg) {

	BlockedBloomFilter bbf(dbg.nbKmers(), 14);

	for (const auto& um : dbg) {

		const string unitig = um.referenceUnitigToString();

        KmerHashIterator<RepHash> it_kmer_h(unitig.c_str(), unitig.length(), dbg.getK()), it_kmer_h_end;
        minHashIterator<RepHash> it_min(unitig.c_str(), unitig.length(), dbg.getK(), dbg.getG(), RepHash(), true);

        for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

            const pair<uint64_t, int> p = *it_kmer_h; // <k-mer hash, k-mer position in sequence>

            it_min += (p.second - it_min.getKmerPosition()); //If one or more k-mer were jumped because contained non-ACGT char.

            bbf.insert(p.first, it_min.getHash(), false);
        }
	}

	return bbf;
}

string retrieveMissingReads(const Correct_Opt& opt){

    Correct_Opt opt_pass_lr(opt);

    opt_pass_lr.k = opt_pass_lr.small_k;

    opt_pass_lr.filename_seq_in.clear();
    opt_pass_lr.filename_seq_in.insert(opt_pass_lr.filename_seq_in.end(), opt_pass_lr.filenames_long_in.begin(), opt_pass_lr.filenames_long_in.end());
    opt_pass_lr.filename_seq_in.insert(opt_pass_lr.filename_seq_in.end(), opt_pass_lr.filenames_helper_long_in.begin(), opt_pass_lr.filenames_helper_long_in.end());

    CompactedDBG<UnitigData> dbg_lr(opt_pass_lr.k);

	const size_t k = dbg_lr.getK();
	const size_t g = dbg_lr.getG();

    std::atomic<size_t> nb_reads_added;

    nb_reads_added = 0;

	string filename_out_extra_sr = opt.filename_long_out + "_extra_sr.fasta";

	if (opt.verbose) cout << "Ratatosk::retrieveMissingReads(): Creating index of short reads" << endl;

	const pair<BlockedBloomFilter, unordered_set<uint64_t, CustomHashUint64_t>> p_bf_sr = buildBBF(opt.filename_seq_in, opt, k, g, false);

	if (opt.verbose) cout << "Ratatosk::retrieveMissingReads(): Creating index of long reads" << endl;

    dbg_lr.build(opt_pass_lr);

    if (opt.verbose) cout << "Ratatosk::retrieveMissingReads(): Adding coverage to long read graph" << endl;

    const size_t nb_km_lr = dbg_lr.nbKmers();

    if (nb_km_lr != 0){

	    BlockedBloomFilter bf_lr(max(nb_km_lr, static_cast<size_t>(1)), opt.nb_bits_unique_kmers_bf);

	    auto addKmersToBBF = [&](CompactedDBG<UnitigData>::iterator it_s, CompactedDBG<UnitigData>::iterator it_e, const bool multi_threaded) {

	        while (it_s != it_e){

	        	const string unitig = it_s->referenceUnitigToString();

	            KmerHashIterator<RepHash> it_kmer_h(unitig.c_str(), unitig.length(), k), it_kmer_h_end;
	        	minHashIterator<RepHash> it_min(unitig.c_str(), unitig.length(), k, g, RepHash(), true);

	            for (; it_kmer_h != it_kmer_h_end; ++it_kmer_h) {

	                const pair<uint64_t, int> p_ = *it_kmer_h; // <k-mer hash, k-mer position in sequence>

	                it_min += (p_.second - it_min.getKmerPosition()); //If one or more k-mer were jumped because contained non-ACGT char.

	                const uint64_t min_hr = it_min.getHash();

	                bf_lr.insert(p_.first, min_hr, multi_threaded);
	            }

	            ++it_s;
	        }
	    };

	    const size_t slice = 1024;

	    CompactedDBG<UnitigData>::iterator it_s = dbg_lr.begin();
	    CompactedDBG<UnitigData>::iterator it_e = dbg_lr.end();

	    if ((opt.nb_threads == 1) || (dbg_lr.size() < slice)) addKmersToBBF(it_s, it_e, false);
	    else {

	        vector<thread> workers;

	        mutex mutex_graph;

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	            workers.emplace_back(

	                [&]{

	                	CompactedDBG<UnitigData>::iterator it_s_l;
	                	CompactedDBG<UnitigData>::iterator it_e_l;

	                	while (true){

	                		{
	                			unique_lock<mutex> lock(mutex_graph);

	                			if (it_s == it_e) return;
	                			else {

	                				it_s_l = it_s;
	                				it_e_l = it_s;

	                				for (size_t i = 0; (i < slice) && (it_e_l != it_e); ++i) ++it_e_l;

	                				it_s = it_e_l;
	                			}
	                		}

	                		addKmersToBBF(it_s_l, it_e_l, true);
	                	}
	                }
	            );
	        }

	        for (auto& t : workers) t.join();
	    }

	    dbg_lr.clear();

		if (opt.verbose) cout << "Ratatosk::retrieveMissingReads(): Querying full short read set for missing reads" << endl;

	    ofstream outfile;
	    ostream out(0);

	    string s;
	    string n;

	    mutex mutex_file_in, mutex_file_out;

		FileParser fp(opt.filenames_short_all);

	    outfile.open(filename_out_extra_sr.c_str());
	    out.rdbuf(outfile.rdbuf());

	    // Main worker thread
	    auto worker_function = [&](char* seq_buf, const size_t seq_buf_sz, const vector<string>& v_read_names) {

	    	size_t i = 0;
	    	size_t l_nb_reads_added = 0;

	        const char* str_end = seq_buf + seq_buf_sz;

	        while (seq_buf < str_end) { // for each input

	            const int len = strlen(seq_buf);
	            const int len_km = len - k + 1;

	            size_t nb_km = 0;
	            size_t remaining_len_km = len_km;

	            toUpperCase(seq_buf, len); // Put characters in upper case

	            KmerHashIterator<RepHash> it_kmer_h(seq_buf, len, k), it_kmer_h_end;
	        	minHashIterator<RepHash> it_min(seq_buf, len, k, g, RepHash(), true);

	            for (; (it_kmer_h != it_kmer_h_end) && (nb_km < opt.min_nb_km_unmapped) && (nb_km + remaining_len_km >= opt.min_nb_km_unmapped); ++it_kmer_h) {

	                const pair<uint64_t, int> p = *it_kmer_h; // <k-mer hash, k-mer position in sequence>

	                it_min += (p.second - it_min.getKmerPosition()); //If one or more k-mer were jumped because contained non-ACGT char.

	                const uint64_t min_hr = it_min.getHash();

	                nb_km += static_cast<size_t>(!p_bf_sr.first.contains(p.first, min_hr) && bf_lr.contains(p.first, min_hr));
	                remaining_len_km = len_km - p.second;
	            }

	            if (nb_km >= opt.min_nb_km_unmapped) {

	        		++l_nb_reads_added;

	        		unique_lock<mutex> lock(mutex_file_out);

	        		out << '>' << v_read_names[i] << '\n' << string(seq_buf) << '\n';
	            }

	            seq_buf += len + 1;
	            ++i;
	        }

	        return l_nb_reads_added;
	    };

	    auto reading_function = [&](char* seq_buf, size_t& seq_buf_sz, vector<string>& v_read_names) {

	        size_t file_id = 0;

	        seq_buf_sz = 0;

	        while (seq_buf_sz < opt.buffer_sz) {

	        	const bool isNewRead = (s.length() == 0);

	            if (!isNewRead || fp.read(s, file_id)) {

	                const size_t len_s = s.length();

	                if (len_s >= k){

	                	if (isNewRead) n = fp.getNameString();

	                    if ((opt.buffer_sz - seq_buf_sz) < (len_s + 1)) break;
	                    else {

	                    	v_read_names.push_back(move(n));

	                        strcpy(seq_buf + seq_buf_sz, s.c_str());

	                        seq_buf_sz += len_s + 1;

	                        s.clear();
	                    }
	                }
	                else s.clear();
	            }
	            else return true;
	        }

	        return false;
	    };

	    {
	        bool stop = false;

	        size_t nb_processed = 0;

	        vector<thread> workers; // need to keep track of threads so we can join them

	        for (size_t t = 0; t < opt.nb_threads; ++t){

	            workers.emplace_back(

	                [&]{

	                    char* buffer_seq = new char[opt.buffer_sz]();

	                    size_t buffer_seq_sz = 0;

	                    vector<string> v_read_names;

	                    while (true) {

	                        {
	                            unique_lock<mutex> lock(mutex_file_in);

	                            if (stop) break;

	                            const size_t nb_processed_before = nb_processed;

	                            stop = reading_function(buffer_seq, buffer_seq_sz, v_read_names);
	                            nb_processed += v_read_names.size();

	                            if ((nb_processed / 1000000) != (nb_processed_before / 1000000)) {

	                            	cout << "Ratatosk::retrieveMissingReads(): " << nb_processed << " short reads queried." << endl;
	                            }
	                        }

	                        nb_reads_added += worker_function(buffer_seq, buffer_seq_sz, v_read_names);
	                        v_read_names.clear();
	                    }

	                    delete[] buffer_seq;
	                }
	            );
	        }

	        for (auto& t : workers) t.join();
	    }

		if (opt.verbose) cout << "Ratatosk::retrieveMissingReads(): Added " << nb_reads_added << " short reads to dataset." << endl;

	    fp.close();
	    outfile.close();
	}

	if (nb_reads_added == 0){

		remove(filename_out_extra_sr.c_str());
		filename_out_extra_sr = "";
	} 

    return filename_out_extra_sr;
}

/*size_t estimateCoverage(const CompactedDBG<UnitigData>& dbg) {

	size_t tot_cov = 0, nb_km = 0;

	const size_t k = dbg.getK();

	for (const auto& um_start : dbg) {

		if (um_start.getData()->isBranching()) {

			vector<const_UnitigMap<UnitigData>> v_um;

			bool branches = false, is_snp = true;

			for (const auto& um_succ : um_start.getSuccessors()){

				v_um.push_back(um_succ);

				is_snp = (is_snp && (um_succ.len == k));
				branches = (branches || um_succ.getData()->isBranching());
			}

			if ((v_um.size() >= 2) && is_snp && !branches) { // At least two non-branching successors of with k k-mers

				const_UnitigMap<UnitigData> um_end;

				bool is_simple_bubble = true;

				for (const auto& um_succ : v_um){

					for (const auto& um_succ_succ : um_succ.getSuccessors()){

						if (um_end.isEmpty) um_end = um_succ_succ;
						else is_simple_bubble = is_simple_bubble && um_succ_succ.isSameReferenceUnitig(um_end) && (um_succ_succ.strand == um_end.strand);
					}
				}

				if (is_simple_bubble) {

					for (const auto& um_succ : v_um){

						nb_km += um_succ.len;
						tot_cov += um_succ.getData()->getCoverage();
					}
				}
			}
		}
	}

	return ((nb_km == 0) ? 0 : (tot_cov / nb_km));
}*/

size_t estimateHaplotypeCoverage(const CompactedDBG<UnitigData>& dbg) {

	size_t tot_cov = 0, nb_km = 0;

	const size_t k = dbg.getK();

	for (const auto& um_start : dbg) {

		if (um_start.getSuccessors().cardinality() > 1) {

			vector<const_UnitigMap<UnitigData>> v_um;

			bool branches = false;

			for (const auto& um_succ : um_start.getSuccessors()){

				v_um.push_back(um_succ);

				branches = (branches || (um_succ.getSuccessors().cardinality() > 1) || (um_succ.getPredecessors().cardinality() > 1));
			}

			if ((v_um.size() >= 2) && !branches) { // At least two non-branching successors of with k k-mers

				const_UnitigMap<UnitigData> um_end;

				bool is_simple_bubble = true;

				for (const auto& um_succ : v_um){

					for (const auto& um_succ_succ : um_succ.getSuccessors()){

						if (um_end.isEmpty) um_end = um_succ_succ;
						else is_simple_bubble = is_simple_bubble && um_succ_succ.isSameReferenceUnitig(um_end) && (um_succ_succ.strand == um_end.strand);
					}
				}

				if (is_simple_bubble) {

					for (const auto& um_succ : v_um){

						nb_km += um_succ.len;
						tot_cov += um_succ.getData()->getCoverage();
					}
				}
			}
		}
	}

	return ((nb_km == 0) ? 0 : (tot_cov / nb_km));
}

/*void expandConnectedComponent(CompactedDBG<UnitigData>& dbg, const size_t id_mapped, const size_t id_visited) { 

	for (const auto& um : dbg){

		// If this is a mapped read that has not been visited
		if (um.getData()->getConnectedComp() == id_mapped) {

			queue<UnitigMap<UnitigData>> q;

			q.push(um);

			while (!q.empty()){

				const UnitigMap<UnitigData> traversed_um = q.front();

				q.pop();

				if (traversed_um.getData()->getConnectedComp() == id_mapped){

					traversed_um.getData()->setConnectedComp(id_visited);

					for (const auto& traversed_um_succ : traversed_um.getSuccessors()) q.push(traversed_um_succ);
					for (const auto& traversed_um_pred : traversed_um.getPredecessors()) q.push(traversed_um_pred);
				}
			}
		}
	}

	return;
}*/

/*Roaring* createPartitions(CompactedDBG<UnitigData>& dbg, const vector<Kmer>& v_centroids, const double max_path_len, const size_t nb_threads, const bool verbose) {

	auto expandPartition = [&](const UnitigMap<UnitigData>& src, const size_t id_partition){

		size_t sum = 0;

		KmerHashTable<uint8_t> km_h;
		queue<UnitigMap<UnitigData>> q1;

		q1.push(src);
		km_h.insert(src.getUnitigHead(), 0);

		while (!q1.empty()){

			const UnitigMap<UnitigData> um = q1.front();

			q1.pop();
			um.getData()->setConnectedComp(id_partition);

			sum += um.len;

			if (sum >= max_path_len) break;

			for (const auto& um_succ : um.getSuccessors()){

				if ((um_succ.getData()->getConnectedComp() == 0) && km_h.insert(um_succ.getUnitigHead(), 0).second) q1.push(um_succ);
			}

			for (const auto& um_pred : um.getPredecessors()){

				if ((um_pred.getData()->getConnectedComp() == 0) && km_h.insert(um_pred.getUnitigHead(), 0).second) q1.push(um_pred);
			}
		}

		return sum;
	};

	auto mergeNeighbors = [&](const UnitigMap<UnitigData>& src, vector<size_t>& v_part_sz){

		const size_t id_part_src = src.getData()->getConnectedComp();

		size_t min_bp_part = 0xffffffffffffffffULL;
		size_t min_id_part = id_part_src;

		Roaring part_neighbors;

		KmerHashTable<uint8_t> km_h;
		queue<UnitigMap<UnitigData>> q1;

		q1.push(src);
		km_h.insert(src.getUnitigHead(), 0);

		while (!q1.empty()){

			const UnitigMap<UnitigData> um = q1.front();

			q1.pop();

			for (const auto& um_succ : um.getSuccessors()){

				if (km_h.insert(um_succ.getUnitigHead(), 0).second){

					const size_t id_part_succ = um_succ.getData()->getConnectedComp();

					if (id_part_succ == id_part_src) q1.push(um_succ);
					else part_neighbors.add(id_part_succ);
				}
			}

			for (const auto& um_pred : um.getPredecessors()){

				if (km_h.insert(um_pred.getUnitigHead(), 0).second){

					const size_t id_part_pred = um_pred.getData()->getConnectedComp();

					if (id_part_pred == id_part_src) q1.push(um_pred);
					else part_neighbors.add(id_part_pred);
				}
			}
		}

		for (const uint32_t part_id : part_neighbors){

			if (v_part_sz[part_id] < min_bp_part) {

				min_bp_part = v_part_sz[part_id];
				min_id_part = part_id;
			}
		}

		if (min_bp_part != 0xffffffffffffffffULL){

			v_part_sz[min_id_part] += v_part_sz[id_part_src];
			v_part_sz[id_part_src] = 0xffffffffffffffffULL;

			q1.push(src);
			km_h.clear();
			km_h.insert(src.getUnitigHead(), 0);

			while (!q1.empty()){

				const UnitigMap<UnitigData> um = q1.front();

				q1.pop();
				um.getData()->setConnectedComp(min_id_part);

				for (const auto& um_succ : um.getSuccessors()){

					if ((um_succ.getData()->getConnectedComp() == id_part_src) && (km_h.insert(um_succ.getUnitigHead(), 0).second)) q1.push(um_succ);
				}

				for (const auto& um_pred : um.getPredecessors()){

					if ((um_pred.getData()->getConnectedComp() == id_part_src) && (km_h.insert(um_pred.getUnitigHead(), 0).second)) q1.push(um_pred);
				}
			}

			return true;
		}

		return false;
	};

	auto getNeighbors = [&](const UnitigMap<UnitigData>& src){

		PairID p_id_part;
		KmerHashTable<uint8_t> km_h;
		queue<UnitigMap<UnitigData>> q1, q2;

		Roaring neighbors;

		const size_t id_part_src = src.getData()->getConnectedComp();

		q1.push(src);
		km_h.insert(src.getUnitigHead(), 0);
		neighbors.add(id_part_src);

		while (!q1.empty()){

			const UnitigMap<UnitigData> um = q1.front();

			q1.pop();

			p_id_part |= um.getData()->getPairID();

			for (const auto& um_succ : um.getSuccessors()){

				if (km_h.insert(um_succ.getUnitigHead(), 0).second){

					if (um_succ.getData()->getConnectedComp() == id_part_src) q1.push(um_succ);
					else q2.push(um_succ);
				}
			}

			for (const auto& um_pred : um.getPredecessors()){

				if (km_h.insert(um_pred.getUnitigHead(), 0).second){

					if (um_pred.getData()->getConnectedComp() == id_part_src) q1.push(um_pred);
					else q2.push(um_pred);
				}
			}
		}

		TinyBloomFilter<uint32_t> tbf_id_part(p_id_part.cardinality(), 14);

		for (const uint32_t id_part : p_id_part) tbf_id_part.insert(id_part);

		while (!q2.empty()){

			const UnitigMap<UnitigData> um = q2.front();

			const PairID& p_id_um = um.getData()->getPairID();
			const size_t part_id_um = um.getData()->getConnectedComp();

			const bool is_already_neighbor = neighbors.contains(part_id_um);

			q2.pop();

			if (p_id_um.isEmpty() || is_already_neighbor || hasEnoughSharedPairID(tbf_id_part, p_id_part, p_id_um, 1)){

				if (!is_already_neighbor) neighbors.add(part_id_um);

				for (const auto& um_succ : um.getSuccessors()){

					if (km_h.insert(um_succ.getUnitigHead(), 0).second) q2.push(um_succ);
				}

				for (const auto& um_pred : um.getPredecessors()){

					if (km_h.insert(um_pred.getUnitigHead(), 0).second) q2.push(um_pred);
				}
			}
		}

		return neighbors;
	};

	vector<Kmer> v_centroids_tmp;
	vector<size_t> v_part_sz;

	// Create partitions from centroids that expand 500 bp in each direction
	size_t id_part = 1;

	const size_t k = dbg.getK();

	v_part_sz.push_back(0);

	if (v_centroids.empty()){

		for (const auto& um : dbg){

			if (um.getData()->getConnectedComp() == 0){

				const size_t sz = expandPartition(um, id_part);

				++id_part;

				v_part_sz.push_back(sz);
				v_centroids_tmp.push_back(um.getUnitigHead());
			}
		}
	}
	else {

		for (const auto km_centroid : v_centroids){

			const UnitigMap<UnitigData> um = dbg.find(km_centroid);

			if (!um.isEmpty && (um.getData()->getConnectedComp() == 0)){

				UnitigMap<UnitigData> um_tmp = um;

				um_tmp.dist = 0;
				um_tmp.len = um.size - k + 1;

				const size_t sz = expandPartition(um_tmp, id_part);

				++id_part;

				v_part_sz.push_back(sz);
			}
		}
	}

	if (verbose) cout << "Ratatosk::createPartitions(): Created " << (id_part - 1) << " partitions." << endl;

	const vector<Kmer>& v_centroids_ref = v_centroids.empty() ? v_centroids_tmp : v_centroids;

	size_t nb_too_small = 0;
	size_t prev_nb_too_small = 0xffffffffffffffffULL;

	for (const auto sz_part : v_part_sz) nb_too_small += static_cast<size_t>(sz_part < max_path_len);

	while ((nb_too_small > 1) && (nb_too_small != prev_nb_too_small)) {

		if (verbose) cout << "Ratatosk::createPartitions(): Merging " << nb_too_small << " partitions." << endl;

		prev_nb_too_small = nb_too_small;

		for (const auto km_centroid : v_centroids_ref){

			const UnitigMap<UnitigData> um = dbg.find(km_centroid);

			if (!um.isEmpty){

				const size_t id_part_um = um.getData()->getConnectedComp();

				if ((id_part_um != 0) && (v_part_sz[id_part_um] < max_path_len)) {

					UnitigMap<UnitigData> um_tmp = um;

					um_tmp.dist = 0;
					um_tmp.len = um.size - k + 1;

					mergeNeighbors(um_tmp, v_part_sz);
				}
			}
		}

		nb_too_small = 0;

		for (const auto sz_part : v_part_sz) nb_too_small += static_cast<size_t>(sz_part < max_path_len);
	}

	Roaring* adjacency = new Roaring[id_part + 1];

	size_t processed_centroid = 0;

	if (nb_threads == 1){

		for (const auto km_centroid : v_centroids_ref){

			const UnitigMap<UnitigData> um = dbg.find(km_centroid);

			if (!um.isEmpty){

				const size_t id_part_um = um.getData()->getConnectedComp();

				if ((id_part_um != 0) && adjacency[id_part_um].isEmpty()){

					UnitigMap<UnitigData> um_tmp = um;

					um_tmp.dist = 0;
					um_tmp.len = um.size - k + 1;

					adjacency[id_part_um] = getNeighbors(um_tmp);

					++processed_centroid;

					if (verbose) cout << "Ratatosk::createPartitions(): Processed " << processed_centroid << " centroids." << endl;
				}
			}
		}
	}
	else {

		const size_t id_unreachable = id_part + 1;

        vector<thread> workers; // need to keep track of threads so we can join them

        LockGraph lck_g(nb_threads * 1024);

        std::atomic<size_t> i;
        std::atomic<size_t> processed_centroid;

        i = 0;
        processed_centroid = 0;

        for (size_t t = 0; t < nb_threads; ++t){

            workers.emplace_back(

                [&]{

                	while (true){

	                	const size_t l_i = i++;

	                	if (l_i >= v_centroids_ref.size()) return;

						const UnitigMap<UnitigData> um = dbg.find(v_centroids_ref[l_i]);

						if (!um.isEmpty){

							const size_t id_part_um = um.getData()->getConnectedComp();

							if (id_part_um != 0){

								lck_g.lock_unitig(id_part_um);

								if (adjacency[id_part_um].isEmpty()) {

									adjacency[id_part_um].add(id_unreachable); // Add fake partition ID that cannot be reached, no other thread cannot process this partition anymore

									lck_g.unlock_unitig(id_part_um);

									UnitigMap<UnitigData> um_tmp = um;

									um_tmp.dist = 0;
									um_tmp.len = um.size - k + 1;

									Roaring r = getNeighbors(um_tmp);

									lck_g.lock_unitig(id_part_um);

									adjacency[id_part_um] = move(r);

									lck_g.unlock_unitig(id_part_um);

									const size_t l_processed_centroid = ++processed_centroid;

									if (verbose && (l_processed_centroid % 100 == 0)) cout << "Ratatosk::createPartitions(): Processed " << l_processed_centroid << " centroids." << endl;
								}
								else lck_g.unlock_unitig(id_part_um);
							}
						}
                	}
                }
			);
        }

        for (auto& t : workers) t.join();

        for (size_t i = 0; i < id_part + 1; ++i) adjacency[i].remove(id_unreachable); // Remove fake partition ID that cannot be reached
	}

	for (size_t i = 0; i < id_part + 1; ++i) adjacency[i].add(0); // Unvisited vertices are always considered to be nearby

	return adjacency;
}*/

// Detect unitigs which are in a short cycle
void detectShortCycles(CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt) {

	auto detectShortCycle = [&opt](const const_UnitigMap<UnitigData>& um, const bool traversal_direction) {

		queue<Path<UnitigData>> q_traversal; // Queue for graph traversal
		vector<string> v_path_out;

		const SharedPairID& spid_start = um.getData()->getPairID();

		const_UnitigMap<UnitigData> um_start = um;

		um_start.strand = traversal_direction;

		// Initialize graph traversal
		{
			Path<UnitigData> p_start;

			p_start.extend(um_start);
			q_traversal.push(move(p_start));
		}

		while (!q_traversal.empty()) {

			const Path<UnitigData> path = q_traversal.front();
			const const_UnitigMap<UnitigData>& um_curr = path.back();

			q_traversal.pop();

			for (const auto& um_succ : um_curr.getSuccessors()) {

				// If the edge (v1,v2) exists in a min. number of reads
				if (um_curr.getData()->getSharedPids(um_curr.strand, um_succ.getMappedHead().toString()[opt.k-1])) {

					// Make sure we follow a unitig which is read-compatible with the start unitig
					if (getNumberSharedPairID(um_curr.getData()->getPairID(), spid_start, opt.min_cov_vertices) >= opt.min_cov_vertices) {

						// Graph traversal came back on the same visited unitig
						if (um_start.isSameReferenceUnitig(um_succ) && (um_succ.strand == um_start.strand)) {

							Path<UnitigData> p_succ(path);

							p_succ.extend(um_succ);

							const vector<const_UnitigMap<UnitigData>> v_um_succ = p_succ.toVector();

							bool noShorterCycle = true;

							// Check if path contains repeated unitigs except start and end unitigs, indicating path contains a smaller cycle in it
							{
								unordered_set<Kmer, KmerHash> us_km;

								for (size_t i = 1; (i < v_um_succ.size() - 1) && noShorterCycle; ++i) noShorterCycle = us_km.insert(v_um_succ[i].getMappedHead()).second;
							}

							if (noShorterCycle) {

								PairID pid = spid_start.toPairID();

								for (size_t i = 1; (i < (v_um_succ.size() - 1)) && (pid.cardinality() >= opt.min_cov_vertices); ++i) pid &= v_um_succ[i].getData()->getPairID().toPairID();

								if (pid.cardinality() >= opt.min_cov_vertices) v_path_out.push_back(p_succ.getMiddleCompactedPath()); // Cycle is supported by at least opt.min_cov_vertices reads
							}
						}
						else if ((path.length() - um_start.len) < (opt.k * 2)) { // Only short cycles are visited

							Path<UnitigData> p_succ(path);

							p_succ.extend(um_succ);
							q_traversal.push(move(p_succ));
						}
					}
				}
			}
		}

		return v_path_out;
	};

	if (opt.verbose) cout << "Ratatosk::detectShortCycles(): Detecting unitigs in short cycles" << endl;

	if (opt.nb_threads == 1){

		size_t count_short_cycles = 0;
		size_t count_unique_short_cycles = 0;

	    for (const auto& um : dbg) {

	    	const vector<string> v_cycles = detectShortCycle(um, true);

	    	count_short_cycles += static_cast<size_t>(v_cycles.size() != 0);
	    	count_unique_short_cycles += static_cast<size_t>(v_cycles.size() == 1);

	    	um.getData()->setIsCycle(!v_cycles.empty());
						    	
	    	if (!v_cycles.empty()){

	    		size_t sum_len_compact_cycles = 0;

	    		for (const auto& s_cycle : v_cycles) sum_len_compact_cycles += s_cycle.length() + 1;

	    		char* cstr_cycles = new char[sum_len_compact_cycles]();

	    		for (size_t i = 0, j = 0; i < v_cycles.size(); ++i) {

	    			memcpy(cstr_cycles + j, v_cycles[i].c_str(), (v_cycles[i].length() + 1) * sizeof(char));
	    			j += (v_cycles[i].length() + 1);
	    		}

	    		um.getData()->setCompactCycles(sum_len_compact_cycles, cstr_cycles);
	    	}
	    }

	    if (opt.verbose) cout << "Ratatosk::detectShortCycles(): Found " << count_short_cycles << " in short cycles. (" << count_unique_short_cycles << " unique)" << endl;
	}
	else {

		const size_t slice = 1024;

        atomic<size_t> count_short_cycles(0);
        atomic<size_t> count_unique_short_cycles(0);

        vector<thread> workers; // need to keep track of threads so we can join them

        mutex mutex_graph;

        CompactedDBG<UnitigData>::iterator it_c = dbg.begin();
        CompactedDBG<UnitigData>::iterator it_e = dbg.end();

        for (size_t t = 0; t < opt.nb_threads; ++t){

            workers.emplace_back(

                [&, t]{

					CompactedDBG<UnitigData>::iterator l_it_c, l_it_e;

                    while (true) {

                        {
                            unique_lock<mutex> lock(mutex_graph);

                            if (it_c == it_e) return;

                            l_it_c = it_c;

					        for (size_t i = 0; (i < slice) && (it_c != it_e); ++i) ++it_c;

							l_it_e = it_c;
                        }

                        {
							size_t l_count_short_cycles = 0;
							size_t l_count_unique_short_cycles = 0;

							while (l_it_c != l_it_e){

						    	const vector<string> v_cycles = detectShortCycle(*l_it_c, true);

	    						l_count_short_cycles += static_cast<size_t>(v_cycles.size() != 0);
	    						l_count_unique_short_cycles += static_cast<size_t>(v_cycles.size() == 1);

	    						l_it_c->getData()->setIsCycle(!v_cycles.empty());
						    	
						    	if (!v_cycles.empty()){

						    		size_t sum_len_compact_cycles = 0;

						    		for (const auto& s_cycle : v_cycles) sum_len_compact_cycles += s_cycle.length() + 1;

						    		char* cstr_cycles = new char[sum_len_compact_cycles]();

						    		for (size_t i = 0, j = 0; i < v_cycles.size(); ++i) {

						    			memcpy(cstr_cycles + j, v_cycles[i].c_str(), (v_cycles[i].length() + 1) * sizeof(char));
						    			j += (v_cycles[i].length() + 1);
						    		}

						    		l_it_c->getData()->setCompactCycles(sum_len_compact_cycles, cstr_cycles);
						    	}

								++l_it_c;
							}

							count_short_cycles += l_count_short_cycles;
							count_unique_short_cycles += l_count_unique_short_cycles;
                        }
                    }
                }
            );
        }

        for (auto& t : workers) t.join();

        if (opt.verbose) cout << "Ratatosk::detectShortCycles(): Found " << count_short_cycles << " in short cycles. (" << count_unique_short_cycles << " unique)" << endl;
	}
}

/*string phasing(const CompactedDBG<UnitigData>& dbg, const Correct_Opt& opt, const string& s, const vector<pair<size_t, const_UnitigMap<UnitigData>>>& v_um_solid) {

	struct CustomHashPair {

	    size_t operator()(const pair<size_t, size_t>& p) const {

	        return XXH64(&p, sizeof(pair<size_t, size_t>), 0);
	    }
	};

	unordered_map<size_t, vector<size_t>> m_pids;
	unordered_map<pair<size_t, size_t>, size_t, CustomHashPair> m_tup;

	vector<pair<pair<size_t, size_t>, size_t>> v_tup;
	vector<size_t> v_all_pids;

	string s_out = s;

    auto comp_pair = [](const pair<pair<size_t, size_t>, size_t>& p1, const pair<pair<size_t, size_t>, size_t>& p2) {

        return (p1.second < p2.second);
    };

	for (size_t i = 0; i < v_um_solid.size(); ++i) {

		const const_UnitigMap<UnitigData>& um = v_um_solid[i].second;

		if (((i==0) || !um.isSameReferenceUnitig(v_um_solid[i-1].second)) && um.getData()->has_ambiguity_char()) {

			const SharedPairID& spid = um.getData()->getPairID().toPairID();

			for (const auto id : spid) m_pids.insert(pair<size_t, vector<size_t>>(id, vector<size_t>())).first->second.push_back(i);

			v_all_pids.push_back(i);
		}
	}

	if (v_all_pids.size() >= 2) {

		for (const auto& pid_occ : m_pids) {

			for (size_t i = 0; i < pid_occ.second.size() - 1; ++i) {

				const pair<size_t, size_t> p = {pid_occ.second[i], pid_occ.second[i+1]};

				m_tup.insert(pair<pair<size_t, size_t>, size_t>(p, 0)).first->second += 1;
			}
		}

		for (size_t i = 0; i < v_all_pids.size() - 1; ++i) {

			const unordered_map<pair<size_t, size_t>, size_t, CustomHashPair>::const_iterator it = m_tup.find({v_all_pids[i], v_all_pids[i+1]});

			if (it != m_tup.end()) v_tup.push_back(*it);
		}

		sort(v_tup.begin(), v_tup.end(), comp_pair);

		for (size_t i = 0; i < 0.05 * v_tup.size(); ++i) {

			s_out.replace(v_um_solid[v_tup[i].first.first].first, opt.k, opt.k, 'N');
			s_out.replace(v_um_solid[v_tup[i].first.second].first, opt.k, opt.k, 'N');

			for (size_t j = v_tup[i].first.first + 1; j < v_um_solid.size(); ++j) {

				if (v_um_solid[j].second.isSameReferenceUnitig(v_um_solid[j-1].second)) s_out.replace(v_um_solid[j].first, opt.k, opt.k, 'N');
				else break;
			}

			for (size_t j = v_tup[i].first.second + 1; j < v_um_solid.size(); ++j) {

				if (v_um_solid[j].second.isSameReferenceUnitig(v_um_solid[j-1].second)) s_out.replace(v_um_solid[j].first, opt.k, opt.k, 'N');
				else break;
			}
		}
	}

	return s_out;
}*/