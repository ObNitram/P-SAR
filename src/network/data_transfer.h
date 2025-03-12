#pragma once


void sync_page(struct page *page);

void transfer_page(struct page *page, struct node_id *sender);