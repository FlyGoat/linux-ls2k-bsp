/*
 * Copyright (c) 2015, Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/prefetch.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <net/busy_poll.h>
#include "en.h"

static inline bool mlx5e_rx_hw_stamp(struct mlx5e_tstamp *tstamp)
{
	return tstamp->hwtstamp_config.rx_filter == HWTSTAMP_FILTER_ALL;
}

int mlx5e_alloc_rx_wqe(struct mlx5e_rq *rq, struct mlx5e_rx_wqe *wqe, u16 ix)
{
	struct sk_buff *skb;
	dma_addr_t dma_addr;

	skb = napi_alloc_skb(rq->cq.napi, rq->wqe_sz);
	if (unlikely(!skb))
		return -ENOMEM;

	dma_addr = dma_map_single(rq->pdev,
				  /* hw start padding */
				  skb->data,
				  /* hw end padding */
				  rq->wqe_sz,
				  DMA_FROM_DEVICE);

	if (unlikely(dma_mapping_error(rq->pdev, dma_addr)))
		goto err_free_skb;

	*((dma_addr_t *)skb->cb) = dma_addr;
	wqe->data.addr = cpu_to_be64(dma_addr);
	wqe->data.lkey = rq->mkey_be;

	rq->skb[ix] = skb;

	return 0;

err_free_skb:
	dev_kfree_skb(skb);

	return -ENOMEM;
}

static inline void
mlx5e_dma_pre_sync_linear_mpwqe(struct device *pdev,
				struct mlx5e_mpw_info *wi,
				u32 wqe_offset, u32 len)
{
	dma_sync_single_for_cpu(pdev, wi->dma_info.addr + wqe_offset,
				len, DMA_FROM_DEVICE);
}

static inline void
mlx5e_dma_pre_sync_fragmented_mpwqe(struct device *pdev,
				    struct mlx5e_mpw_info *wi,
				    u32 wqe_offset, u32 len)
{
	/* No dma pre sync for fragmented MPWQE */
}

static inline void
mlx5e_add_skb_frag_linear_mpwqe(struct device *pdev,
				struct sk_buff *skb,
				struct mlx5e_mpw_info *wi,
				u32 page_idx, u32 frag_offset,
				u32 len)
{
	unsigned int truesize =	ALIGN(len, MLX5_MPWRQ_STRIDE_SIZE);

	wi->skbs_frags[page_idx]++;
	skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
			&wi->dma_info.page[page_idx], frag_offset,
			len, truesize);
}

static inline void
mlx5e_add_skb_frag_fragmented_mpwqe(struct device *pdev,
				    struct sk_buff *skb,
				    struct mlx5e_mpw_info *wi,
				    u32 page_idx, u32 frag_offset,
				    u32 len)
{
	unsigned int truesize =	ALIGN(len, MLX5_MPWRQ_STRIDE_SIZE);

	dma_sync_single_for_cpu(pdev,
				wi->umr.dma_info[page_idx].addr + frag_offset,
				len, DMA_FROM_DEVICE);
	wi->skbs_frags[page_idx]++;
	skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
			wi->umr.dma_info[page_idx].page, frag_offset,
			len, truesize);
}

static inline void
mlx5e_copy_skb_header_linear_mpwqe(struct device *pdev,
				   struct sk_buff *skb,
				   struct mlx5e_mpw_info *wi,
				   u32 page_idx, u32 offset,
				   u32 headlen)
{
	struct page *page = &wi->dma_info.page[page_idx];

	skb_copy_to_linear_data(skb, page_address(page) + offset,
				ALIGN(headlen, sizeof(long)));
}

static inline void
mlx5e_copy_skb_header_fragmented_mpwqe(struct device *pdev,
				       struct sk_buff *skb,
				       struct mlx5e_mpw_info *wi,
				       u32 page_idx, u32 offset,
				       u32 headlen)
{
	u16 headlen_pg = min_t(u32, headlen, PAGE_SIZE - offset);
	struct mlx5e_dma_info *dma_info = &wi->umr.dma_info[page_idx];
	unsigned int len;

	 /* Aligning len to sizeof(long) optimizes memcpy performance */
	len = ALIGN(headlen_pg, sizeof(long));
	dma_sync_single_for_cpu(pdev, dma_info->addr + offset, len,
				DMA_FROM_DEVICE);
	skb_copy_to_linear_data_offset(skb, 0,
				       page_address(dma_info->page) + offset,
				       len);
#if (MLX5_MPWRQ_SMALL_PACKET_THRESHOLD >= MLX5_MPWRQ_STRIDE_SIZE)
	if (unlikely(offset + headlen > PAGE_SIZE)) {
		dma_info++;
		headlen_pg = len;
		len = ALIGN(headlen - headlen_pg, sizeof(long));
		dma_sync_single_for_cpu(pdev, dma_info->addr, len,
					DMA_FROM_DEVICE);
		skb_copy_to_linear_data_offset(skb, headlen_pg,
					       page_address(dma_info->page),
					       len);
	}
#endif
}

static u16 mlx5e_get_wqe_mtt_offset(u16 rq_ix, u16 wqe_ix)
{
	return rq_ix * MLX5_CHANNEL_MAX_NUM_MTTS +
		wqe_ix * ALIGN(MLX5_MPWRQ_PAGES_PER_WQE, 8);
}

static void mlx5e_build_umr_wqe(struct mlx5e_rq *rq,
				struct mlx5e_sq *sq,
				struct mlx5e_umr_wqe *wqe,
				u16 ix)
{
	struct mlx5_wqe_ctrl_seg      *cseg = &wqe->ctrl;
	struct mlx5_wqe_umr_ctrl_seg *ucseg = &wqe->uctrl;
	struct mlx5_wqe_data_seg      *dseg = &wqe->data;
	struct mlx5e_mpw_info *wi = &rq->wqe_info[ix];
	u8 ds_cnt = DIV_ROUND_UP(sizeof(*wqe), MLX5_SEND_WQE_DS);
	u16 umr_wqe_mtt_offset = mlx5e_get_wqe_mtt_offset(rq->ix, ix);

	memset(wqe, 0, sizeof(*wqe));
	cseg->opmod_idx_opcode =
		cpu_to_be32((sq->pc << MLX5_WQE_CTRL_WQE_INDEX_SHIFT) |
			    MLX5_OPCODE_UMR);
	cseg->qpn_ds    = cpu_to_be32((sq->sqn << MLX5_WQE_CTRL_QPN_SHIFT) |
				      ds_cnt);
	cseg->fm_ce_se  = MLX5_WQE_CTRL_CQ_UPDATE;
	cseg->imm       = rq->umr_mkey_be;

	ucseg->flags = MLX5_UMR_TRANSLATION_OFFSET_EN;
	ucseg->klm_octowords =
		cpu_to_be16(mlx5e_get_mtt_octw(MLX5_MPWRQ_PAGES_PER_WQE));
	ucseg->bsf_octowords =
		cpu_to_be16(mlx5e_get_mtt_octw(umr_wqe_mtt_offset));
	ucseg->mkey_mask     = cpu_to_be64(MLX5_MKEY_MASK_FREE);

	dseg->lkey = sq->mkey_be;
	dseg->addr = cpu_to_be64(wi->umr.mtt_addr);
}

static void mlx5e_post_umr_wqe(struct mlx5e_rq *rq, u16 ix)
{
	struct mlx5e_sq *sq = &rq->channel->icosq;
	struct mlx5_wq_cyc *wq = &sq->wq;
	struct mlx5e_umr_wqe *wqe;
	u8 num_wqebbs = DIV_ROUND_UP(sizeof(*wqe), MLX5_SEND_WQE_BB);
	u16 pi;

	/* fill sq edge with nops to avoid wqe wrap around */
	while ((pi = (sq->pc & wq->sz_m1)) > sq->edge) {
		sq->ico_wqe_info[pi].opcode = MLX5_OPCODE_NOP;
		sq->ico_wqe_info[pi].num_wqebbs = 1;
		mlx5e_send_nop(sq, true);
	}

	wqe = mlx5_wq_cyc_get_wqe(wq, pi);
	mlx5e_build_umr_wqe(rq, sq, wqe, ix);
	sq->ico_wqe_info[pi].opcode = MLX5_OPCODE_UMR;
	sq->ico_wqe_info[pi].num_wqebbs = num_wqebbs;
	sq->pc += num_wqebbs;
	mlx5e_tx_notify_hw(sq, &wqe->ctrl, 0);
}

static inline int mlx5e_get_wqe_mtt_sz(void)
{
	/* UMR copies MTTs in units of MLX5_UMR_MTT_ALIGNMENT bytes.
	 * To avoid copying garbage after the mtt array, we allocate
	 * a little more.
	 */
	return ALIGN(MLX5_MPWRQ_PAGES_PER_WQE * sizeof(__be64),
		     MLX5_UMR_MTT_ALIGNMENT);
}

static int mlx5e_alloc_and_map_page(struct mlx5e_rq *rq,
				    struct mlx5e_mpw_info *wi,
				    int i)
{
	struct page *page;

	page = dev_alloc_page();
	if (unlikely(!page))
		return -ENOMEM;

	wi->umr.dma_info[i].page = page;
	wi->umr.dma_info[i].addr = dma_map_page(rq->pdev, page, 0, PAGE_SIZE,
						PCI_DMA_FROMDEVICE);
	if (unlikely(dma_mapping_error(rq->pdev, wi->umr.dma_info[i].addr))) {
		put_page(page);
		return -ENOMEM;
	}
	wi->umr.mtt[i] = cpu_to_be64(wi->umr.dma_info[i].addr | MLX5_EN_WR);

	return 0;
}

static int mlx5e_alloc_rx_fragmented_mpwqe(struct mlx5e_rq *rq,
					   struct mlx5e_rx_wqe *wqe,
					   u16 ix)
{
	struct mlx5e_mpw_info *wi = &rq->wqe_info[ix];
	int mtt_sz = mlx5e_get_wqe_mtt_sz();
	u32 dma_offset = mlx5e_get_wqe_mtt_offset(rq->ix, ix) << PAGE_SHIFT;
	int i;

	wi->umr.dma_info = kmalloc(sizeof(*wi->umr.dma_info) *
				   MLX5_MPWRQ_PAGES_PER_WQE,
				   GFP_ATOMIC);
	if (unlikely(!wi->umr.dma_info))
		goto err_out;

	/* We allocate more than mtt_sz as we will align the pointer */
	wi->umr.mtt_no_align = kzalloc(mtt_sz + MLX5_UMR_ALIGN - 1,
				       GFP_ATOMIC);
	if (unlikely(!wi->umr.mtt_no_align))
		goto err_free_umr;

	wi->umr.mtt = PTR_ALIGN(wi->umr.mtt_no_align, MLX5_UMR_ALIGN);
	wi->umr.mtt_addr = dma_map_single(rq->pdev, wi->umr.mtt, mtt_sz,
					  PCI_DMA_TODEVICE);
	if (unlikely(dma_mapping_error(rq->pdev, wi->umr.mtt_addr)))
		goto err_free_mtt;

	for (i = 0; i < MLX5_MPWRQ_PAGES_PER_WQE; i++) {
		if (unlikely(mlx5e_alloc_and_map_page(rq, wi, i)))
			goto err_unmap;
		atomic_add(MLX5_MPWRQ_STRIDES_PER_PAGE,
			   &wi->umr.dma_info[i].page->_count);
		wi->skbs_frags[i] = 0;
	}

	wi->consumed_strides = 0;
	wi->dma_pre_sync = mlx5e_dma_pre_sync_fragmented_mpwqe;
	wi->add_skb_frag = mlx5e_add_skb_frag_fragmented_mpwqe;
	wi->copy_skb_header = mlx5e_copy_skb_header_fragmented_mpwqe;
	wi->free_wqe     = mlx5e_free_rx_fragmented_mpwqe;
	wqe->data.lkey = rq->umr_mkey_be;
	wqe->data.addr = cpu_to_be64(dma_offset);

	return 0;

err_unmap:
	while (--i >= 0) {
		dma_unmap_page(rq->pdev, wi->umr.dma_info[i].addr, PAGE_SIZE,
			       PCI_DMA_FROMDEVICE);
		atomic_sub(MLX5_MPWRQ_STRIDES_PER_PAGE,
			   &wi->umr.dma_info[i].page->_count);
		put_page(wi->umr.dma_info[i].page);
	}
	dma_unmap_single(rq->pdev, wi->umr.mtt_addr, mtt_sz, PCI_DMA_TODEVICE);

err_free_mtt:
	kfree(wi->umr.mtt_no_align);

err_free_umr:
	kfree(wi->umr.dma_info);

err_out:
	return -ENOMEM;
}

void mlx5e_free_rx_fragmented_mpwqe(struct mlx5e_rq *rq,
				    struct mlx5e_mpw_info *wi)
{
	int mtt_sz = mlx5e_get_wqe_mtt_sz();
	int i;

	for (i = 0; i < MLX5_MPWRQ_PAGES_PER_WQE; i++) {
		dma_unmap_page(rq->pdev, wi->umr.dma_info[i].addr, PAGE_SIZE,
			       PCI_DMA_FROMDEVICE);
		atomic_sub(MLX5_MPWRQ_STRIDES_PER_PAGE - wi->skbs_frags[i],
			   &wi->umr.dma_info[i].page->_count);
		put_page(wi->umr.dma_info[i].page);
	}
	dma_unmap_single(rq->pdev, wi->umr.mtt_addr, mtt_sz, PCI_DMA_TODEVICE);
	kfree(wi->umr.mtt_no_align);
	kfree(wi->umr.dma_info);
}

void mlx5e_post_rx_fragmented_mpwqe(struct mlx5e_rq *rq)
{
	struct mlx5_wq_ll *wq = &rq->wq;
	struct mlx5e_rx_wqe *wqe = mlx5_wq_ll_get_wqe(wq, wq->head);

	clear_bit(MLX5E_RQ_STATE_UMR_WQE_IN_PROGRESS, &rq->state);
	mlx5_wq_ll_push(wq, be16_to_cpu(wqe->next.next_wqe_index));
	rq->stats.mpwqe_frag++;

	/* ensure wqes are visible to device before updating doorbell record */
	dma_wmb();

	mlx5_wq_ll_update_db_record(wq);
}

static int mlx5e_alloc_rx_linear_mpwqe(struct mlx5e_rq *rq,
				       struct mlx5e_rx_wqe *wqe,
				       u16 ix)
{
	struct mlx5e_mpw_info *wi = &rq->wqe_info[ix];
	gfp_t gfp_mask;
	int i;

	gfp_mask = GFP_ATOMIC | __GFP_COLD | __GFP_MEMALLOC;
	wi->dma_info.page = alloc_pages_node(NUMA_NO_NODE, gfp_mask,
					     MLX5_MPWRQ_WQE_PAGE_ORDER);
	if (unlikely(!wi->dma_info.page))
		return -ENOMEM;

	wi->dma_info.addr = dma_map_page(rq->pdev, wi->dma_info.page, 0,
					 rq->wqe_sz, PCI_DMA_FROMDEVICE);
	if (unlikely(dma_mapping_error(rq->pdev, wi->dma_info.addr))) {
		put_page(wi->dma_info.page);
		return -ENOMEM;
	}

	/* We split the high-order page into order-0 ones and manage their
	 * reference counter to minimize the memory held by small skb fragments
	 */
	split_page(wi->dma_info.page, MLX5_MPWRQ_WQE_PAGE_ORDER);
	for (i = 0; i < MLX5_MPWRQ_PAGES_PER_WQE; i++) {
		atomic_add(MLX5_MPWRQ_STRIDES_PER_PAGE,
			   &wi->dma_info.page[i]._count);
		wi->skbs_frags[i] = 0;
	}

	wi->consumed_strides = 0;
	wi->dma_pre_sync = mlx5e_dma_pre_sync_linear_mpwqe;
	wi->add_skb_frag = mlx5e_add_skb_frag_linear_mpwqe;
	wi->copy_skb_header = mlx5e_copy_skb_header_linear_mpwqe;
	wi->free_wqe     = mlx5e_free_rx_linear_mpwqe;
	wqe->data.lkey = rq->mkey_be;
	wqe->data.addr = cpu_to_be64(wi->dma_info.addr);

	return 0;
}

void mlx5e_free_rx_linear_mpwqe(struct mlx5e_rq *rq,
				struct mlx5e_mpw_info *wi)
{
	int i;

	dma_unmap_page(rq->pdev, wi->dma_info.addr, rq->wqe_sz,
		       PCI_DMA_FROMDEVICE);
	for (i = 0; i < MLX5_MPWRQ_PAGES_PER_WQE; i++) {
		atomic_sub(MLX5_MPWRQ_STRIDES_PER_PAGE - wi->skbs_frags[i],
			   &wi->dma_info.page[i]._count);
		put_page(&wi->dma_info.page[i]);
	}
}

int mlx5e_alloc_rx_mpwqe(struct mlx5e_rq *rq, struct mlx5e_rx_wqe *wqe, u16 ix)
{
	int err;

	err = mlx5e_alloc_rx_linear_mpwqe(rq, wqe, ix);
	if (unlikely(err)) {
		err = mlx5e_alloc_rx_fragmented_mpwqe(rq, wqe, ix);
		if (unlikely(err))
			return err;
		set_bit(MLX5E_RQ_STATE_UMR_WQE_IN_PROGRESS, &rq->state);
		mlx5e_post_umr_wqe(rq, ix);
		return -EBUSY;
	}

	return 0;
}

#define RQ_CANNOT_POST(rq) \
		(!test_bit(MLX5E_RQ_STATE_POST_WQES_ENABLE, &rq->state) || \
		 test_bit(MLX5E_RQ_STATE_UMR_WQE_IN_PROGRESS, &rq->state))

bool mlx5e_post_rx_wqes(struct mlx5e_rq *rq)
{
	struct mlx5_wq_ll *wq = &rq->wq;

	if (unlikely(RQ_CANNOT_POST(rq)))
		return false;

	while (!mlx5_wq_ll_is_full(wq)) {
		struct mlx5e_rx_wqe *wqe = mlx5_wq_ll_get_wqe(wq, wq->head);
		int err;

		err = rq->alloc_wqe(rq, wqe, wq->head);
		if (unlikely(err)) {
			if (err != -EBUSY)
				rq->stats.buff_alloc_err++;
			break;
		}

		mlx5_wq_ll_push(wq, be16_to_cpu(wqe->next.next_wqe_index));
	}

	/* ensure wqes are visible to device before updating doorbell record */
	dma_wmb();

	mlx5_wq_ll_update_db_record(wq);

	return !mlx5_wq_ll_is_full(wq);
}

static void mlx5e_lro_update_hdr(struct sk_buff *skb, struct mlx5_cqe64 *cqe,
				 u32 cqe_bcnt)
{
	struct ethhdr	*eth	= (struct ethhdr *)(skb->data);
	struct iphdr	*ipv4	= (struct iphdr *)(skb->data + ETH_HLEN);
	struct ipv6hdr	*ipv6	= (struct ipv6hdr *)(skb->data + ETH_HLEN);
	struct tcphdr	*tcp;

	u8 l4_hdr_type = get_cqe_l4_hdr_type(cqe);
	int tcp_ack = ((CQE_L4_HDR_TYPE_TCP_ACK_NO_DATA  == l4_hdr_type) ||
		       (CQE_L4_HDR_TYPE_TCP_ACK_AND_DATA == l4_hdr_type));

	u16 tot_len = cqe_bcnt - ETH_HLEN;

	if (eth->h_proto == htons(ETH_P_IP)) {
		tcp = (struct tcphdr *)(skb->data + ETH_HLEN +
					sizeof(struct iphdr));
		ipv6 = NULL;
		skb_shinfo(skb)->gso_type = SKB_GSO_TCPV4;
	} else {
		tcp = (struct tcphdr *)(skb->data + ETH_HLEN +
					sizeof(struct ipv6hdr));
		ipv4 = NULL;
		skb_shinfo(skb)->gso_type = SKB_GSO_TCPV6;
	}

	if (get_cqe_lro_tcppsh(cqe))
		tcp->psh                = 1;

	if (tcp_ack) {
		tcp->ack                = 1;
		tcp->ack_seq            = cqe->lro_ack_seq_num;
		tcp->window             = cqe->lro_tcp_win;
	}

	if (ipv4) {
		ipv4->ttl               = cqe->lro_min_ttl;
		ipv4->tot_len           = cpu_to_be16(tot_len);
		ipv4->check             = 0;
		ipv4->check             = ip_fast_csum((unsigned char *)ipv4,
						       ipv4->ihl);
	} else {
		ipv6->hop_limit         = cqe->lro_min_ttl;
		ipv6->payload_len       = cpu_to_be16(tot_len -
						      sizeof(struct ipv6hdr));
	}
}

static inline void mlx5e_skb_set_hash(struct mlx5_cqe64 *cqe,
				      struct sk_buff *skb)
{
	u8 cht = cqe->rss_hash_type;
	int ht = (cht & CQE_RSS_HTYPE_L4) ? PKT_HASH_TYPE_L4 :
		 (cht & CQE_RSS_HTYPE_IP) ? PKT_HASH_TYPE_L3 :
					    PKT_HASH_TYPE_NONE;
	skb_set_hash(skb, be32_to_cpu(cqe->rss_hash_result), ht);
}

static inline bool is_first_ethertype_ip(struct sk_buff *skb)
{
	__be16 ethertype = ((struct ethhdr *)skb->data)->h_proto;

	return (ethertype == htons(ETH_P_IP) || ethertype == htons(ETH_P_IPV6));
}

static inline void mlx5e_handle_csum(struct net_device *netdev,
				     struct mlx5_cqe64 *cqe,
				     struct mlx5e_rq *rq,
				     struct sk_buff *skb,
				     bool   lro)
{
	if (unlikely(!(netdev->features & NETIF_F_RXCSUM)))
		goto csum_none;

	if (lro) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		return;
	}

	if (is_first_ethertype_ip(skb)) {
		skb->ip_summed = CHECKSUM_COMPLETE;
		skb->csum = csum_unfold((__force __sum16)cqe->check_sum);
		rq->stats.csum_sw++;
		return;
	}

	if (likely((cqe->hds_ip_ext & CQE_L3_OK) &&
		   (cqe->hds_ip_ext & CQE_L4_OK))) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (cqe_is_tunneled(cqe)) {
			skb->csum_level = 1;
			skb->encapsulation = 1;
			rq->stats.csum_inner++;
		}
		return;
	}
csum_none:
	skb->ip_summed = CHECKSUM_NONE;
	rq->stats.csum_none++;
}

static inline void mlx5e_build_rx_skb(struct mlx5_cqe64 *cqe,
				      u32 cqe_bcnt,
				      struct mlx5e_rq *rq,
				      struct sk_buff *skb)
{
	struct net_device *netdev = rq->netdev;
	struct mlx5e_tstamp *tstamp = rq->tstamp;
	int lro_num_seg;

	lro_num_seg = be32_to_cpu(cqe->srqn) >> 24;
	if (lro_num_seg > 1) {
		mlx5e_lro_update_hdr(skb, cqe, cqe_bcnt);
		skb_shinfo(skb)->gso_size = DIV_ROUND_UP(cqe_bcnt, lro_num_seg);
		rq->stats.lro_packets++;
		rq->stats.lro_bytes += cqe_bcnt;
	}

	if (unlikely(mlx5e_rx_hw_stamp(tstamp)))
		mlx5e_fill_hwstamp(tstamp, get_cqe_ts(cqe), skb_hwtstamps(skb));

	skb_record_rx_queue(skb, rq->ix);

	if (likely(netdev->features & NETIF_F_RXHASH))
		mlx5e_skb_set_hash(cqe, skb);

	if (cqe_has_vlan(cqe))
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
				       be16_to_cpu(cqe->vlan_info));

	mlx5e_handle_csum(netdev, cqe, rq, skb, !!lro_num_seg);
	skb->protocol = eth_type_trans(skb, netdev);
}

static inline void mlx5e_complete_rx_cqe(struct mlx5e_rq *rq,
					 struct mlx5_cqe64 *cqe,
					 u32 cqe_bcnt,
					 struct sk_buff *skb)
{
	rq->stats.packets++;
	rq->stats.bytes += cqe_bcnt;
	mlx5e_build_rx_skb(cqe, cqe_bcnt, rq, skb);
	napi_gro_receive(rq->cq.napi, skb);
}

void mlx5e_handle_rx_cqe(struct mlx5e_rq *rq, struct mlx5_cqe64 *cqe)
{
	struct mlx5e_rx_wqe *wqe;
	struct sk_buff *skb;
	__be16 wqe_counter_be;
	u16 wqe_counter;
	u32 cqe_bcnt;

	wqe_counter_be = cqe->wqe_counter;
	wqe_counter    = be16_to_cpu(wqe_counter_be);
	wqe            = mlx5_wq_ll_get_wqe(&rq->wq, wqe_counter);
	skb            = rq->skb[wqe_counter];
	prefetch(skb->data);
	rq->skb[wqe_counter] = NULL;

	dma_unmap_single(rq->pdev,
			 *((dma_addr_t *)skb->cb),
			 rq->wqe_sz,
			 DMA_FROM_DEVICE);

	if (unlikely((cqe->op_own >> 4) != MLX5_CQE_RESP_SEND)) {
		rq->stats.wqe_err++;
		dev_kfree_skb(skb);
		goto wq_ll_pop;
	}

	cqe_bcnt = be32_to_cpu(cqe->byte_cnt);
	skb_put(skb, cqe_bcnt);

	mlx5e_complete_rx_cqe(rq, cqe, cqe_bcnt, skb);

wq_ll_pop:
	mlx5_wq_ll_pop(&rq->wq, wqe_counter_be,
		       &wqe->next.next_wqe_index);
}

static inline void mlx5e_mpwqe_fill_rx_skb(struct mlx5e_rq *rq,
					   struct mlx5_cqe64 *cqe,
					   struct mlx5e_mpw_info *wi,
					   u32 cqe_bcnt,
					   struct sk_buff *skb)
{
	u32 consumed_bytes = ALIGN(cqe_bcnt, MLX5_MPWRQ_STRIDE_SIZE);
	u16 stride_ix      = mpwrq_get_cqe_stride_index(cqe);
	u32 wqe_offset     = stride_ix * MLX5_MPWRQ_STRIDE_SIZE;
	u32 head_offset    = wqe_offset & (PAGE_SIZE - 1);
	u32 page_idx       = wqe_offset >> PAGE_SHIFT;
	u32 head_page_idx  = page_idx;
	u16 headlen = min_t(u16, MLX5_MPWRQ_SMALL_PACKET_THRESHOLD, cqe_bcnt);
	u32 frag_offset    = head_offset + headlen;
	u16 byte_cnt       = cqe_bcnt - headlen;

#if (MLX5_MPWRQ_SMALL_PACKET_THRESHOLD >= MLX5_MPWRQ_STRIDE_SIZE)
	if (unlikely(frag_offset >= PAGE_SIZE)) {
		page_idx++;
		frag_offset -= PAGE_SIZE;
	}
#endif
	wi->dma_pre_sync(rq->pdev, wi, wqe_offset, consumed_bytes);

	while (byte_cnt) {
		u32 pg_consumed_bytes =
			min_t(u32, PAGE_SIZE - frag_offset, byte_cnt);

		wi->add_skb_frag(rq->pdev, skb, wi, page_idx, frag_offset,
				 pg_consumed_bytes);
		byte_cnt -= pg_consumed_bytes;
		frag_offset = 0;
		page_idx++;
	}
	/* copy header */
	wi->copy_skb_header(rq->pdev, skb, wi, head_page_idx, head_offset,
			    headlen);
	/* skb linear part was allocated with headlen and aligned to long */
	skb->tail += headlen;
	skb->len  += headlen;
}

void mlx5e_handle_rx_cqe_mpwrq(struct mlx5e_rq *rq, struct mlx5_cqe64 *cqe)
{
	u16 cstrides       = mpwrq_get_cqe_consumed_strides(cqe);
	u16 wqe_id         = be16_to_cpu(cqe->wqe_id);
	struct mlx5e_mpw_info *wi = &rq->wqe_info[wqe_id];
	struct mlx5e_rx_wqe  *wqe = mlx5_wq_ll_get_wqe(&rq->wq, wqe_id);
	struct sk_buff *skb;
	u16 cqe_bcnt;

	wi->consumed_strides += cstrides;

	if (unlikely((cqe->op_own >> 4) != MLX5_CQE_RESP_SEND)) {
		rq->stats.wqe_err++;
		goto mpwrq_cqe_out;
	}

	if (unlikely(mpwrq_is_filler_cqe(cqe))) {
		rq->stats.mpwqe_filler++;
		goto mpwrq_cqe_out;
	}

	skb = napi_alloc_skb(rq->cq.napi,
			     ALIGN(MLX5_MPWRQ_SMALL_PACKET_THRESHOLD,
				   sizeof(long)));
	if (unlikely(!skb)) {
		rq->stats.buff_alloc_err++;
		goto mpwrq_cqe_out;
	}

	prefetch(skb->data);
	cqe_bcnt = mpwrq_get_cqe_byte_cnt(cqe);

	mlx5e_mpwqe_fill_rx_skb(rq, cqe, wi, cqe_bcnt, skb);
	mlx5e_complete_rx_cqe(rq, cqe, cqe_bcnt, skb);

mpwrq_cqe_out:
	if (likely(wi->consumed_strides < MLX5_MPWRQ_NUM_STRIDES))
		return;

	wi->free_wqe(rq, wi);
	mlx5_wq_ll_pop(&rq->wq, cqe->wqe_id, &wqe->next.next_wqe_index);
}

int mlx5e_poll_rx_cq(struct mlx5e_cq *cq, int budget)
{
	struct mlx5e_rq *rq = container_of(cq, struct mlx5e_rq, cq);
	int work_done;

	for (work_done = 0; work_done < budget; work_done++) {
		struct mlx5_cqe64 *cqe = mlx5e_get_cqe(cq);

		if (!cqe)
			break;

		mlx5_cqwq_pop(&cq->wq);

		rq->handle_rx_cqe(rq, cqe);
	}

	mlx5_cqwq_update_db_record(&cq->wq);

	/* ensure cq space is freed before enabling more cqes */
	wmb();

	return work_done;
}
