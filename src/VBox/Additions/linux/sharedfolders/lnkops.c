#include "vfsmod.h"

static void *sf_follow_link(struct dentry *dentry, struct nameidata *nd)
{
        struct inode *inode = dentry->d_inode;
        struct sf_glob_info *sf_g = GET_GLOB_INFO(inode->i_sb);
        struct sf_inode_info *sf_i = GET_INODE_INFO(inode);
        int error = -ENOMEM;
        unsigned long page = get_zeroed_page(GFP_KERNEL);
        int rc;

        if (page)
        {
                error = 0;
                rc = vboxReadLink(&client_handle, &sf_g->map, sf_i->path, PATH_MAX, (char *)page);
                if (RT_FAILURE(rc))
                {
                        LogFunc(("vboxReadLink failed, caller=%s, rc=%Rrc\n",
                                 __func__, rc));
                        error = -EPROTO;
                }
        }
        nd_set_link(nd, error ? ERR_PTR(error) : (char *)page);
        return NULL;
}

static void sf_put_link(struct dentry *dentry, struct nameidata *nd, void *cookie)
{
        char *page = nd_get_link(nd);
        if (!IS_ERR(page))
                free_page((unsigned long)page);
}

struct inode_operations sf_lnk_iops =
{
        .readlink       = generic_readlink,
        .follow_link    = sf_follow_link,
        .put_link       = sf_put_link
};
