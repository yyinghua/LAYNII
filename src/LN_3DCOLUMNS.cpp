

#include "../dep/laynii_lib.h"

int show_help(void) {
    printf(
    "LN_3DCOLUMNS : Calculates cortical distances (columnar structures) \n"
    "               based on the gray matter (GM) geometry.\n"
    "\n"
    "Usage:\n"
    "    LN_3DCOLUMNS -layer_file layers.nii -landmarks landmarks.nii \n"
    "\n"
    "Options:\n"
    "    -help         : Show this help.\n"
    "    -layer_file   : Nifti (.nii) file containing layer or column masks \n"
    "    -landmarks    : Nifti (.nii) file with landmarks 1, 2, 3 (1 is in \n"
    "                    the center 2 and 3 are the borders. Landmarks \n"
    "                    should be at least 4 voxels thick.\n"
    "    -twodim       : (Optional) Run in 2D only.\n"
    "    -mask         : (Optional) Mask activity outside of layers.\n"
    "    -vinc         : Number of columns.\n"
    "    -jiajiaoption : Include cerebrospinal fluid (CSF). Only do this \n"
    "                    if two sides of the sulcus are not touching. \n"
    "\n"
    "Notes:\n"
    "     - Layer nifti and landmarks nifti should have the same dimensions \n"
    "     - This program now supports INT16, INT32 and FLOAT23 \n"
    "\n");
    return 0;
}

int main(int argc, char * argv[]) {
    char* fin_layer = NULL, * fin_landmark = NULL;
    int ac, twodim = 0, jiajiavinc_max = 45, jiajiaoption = 0;
    if (argc < 3) return show_help();

    // Process user options
    for (ac = 1; ac < argc; ac++) {
        if (!strncmp(argv[ac], "-h", 2)) {
            return show_help();
        } else if (!strcmp(argv[ac], "-layer_file")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -layer_file\n");
                return 1;
            }
            fin_layer = argv[ac];
        } else if (!strcmp(argv[ac], "-landmarks")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -input\n");
                return 1;
            }
            fin_landmark = argv[ac];
        } else if (!strcmp(argv[ac], "-twodim")) {
            twodim = 1;
            cout << "Smoothing only in 2D." << endl;
        } else if (!strcmp(argv[ac], "-jiajiaoption")) {
            jiajiaoption = 1;
            cout << "Do not remove CSF." << endl;
        } else if (!strcmp(argv[ac], "-vinc")) {
            if (++ac >= argc) {
                fprintf(stderr, "** missing argument for -vinc\n");
                return 1;
            }
            jiajiavinc_max = atof(argv[ac]);
        } else {
            fprintf(stderr, "** invalid option, '%s'\n", argv[ac]);
            return 1;
        }
    }
    if (!fin_landmark) {
        fprintf(stderr, "** missing option '-landmarks'\n");
        return 1;
    }
    if (!fin_layer) {
        fprintf(stderr, "** missing option '-layer_file'\n");
        return 1;
    }

    // Read input dataset
    nifti_image * nii_landmark_r = nifti_image_read(fin_landmark, 1);
    if (!nii_landmark_r) {
        fprintf(stderr, "** failed to read NIfTI from '%s'\n", fin_landmark);
        return 2;
    }
    nifti_image * nii_layer_r = nifti_image_read(fin_layer, 1);
    if (!nii_layer_r) {
        fprintf(stderr, "** failed to read NIfTI from '%s'\n", fin_layer);
        return 2;
    }

    log_welcome("LN_3DCOLUMNS");
    log_nifti_descriptives(nii_layer_r);
    log_nifti_descriptives(nii_landmark_r);

    // Get dimensions of input
    int size_z = nii_layer_r->nz;
    int size_x = nii_layer_r->nx;
    int size_y = nii_layer_r->ny;
    int size_t = nii_layer_r->nt;
    int nr_voxels = nii_layer_r->nvox;
    int nx = nii_layer_r->nx;
    int nxy = nii_layer_r->nx * nii_layer_r->ny;
    int nxyz = nii_layer_r->nx * nii_layer_r->ny * nii_layer_r->nz;
    float dX = nii_layer_r->pixdim[1];
    float dY = nii_layer_r->pixdim[2];
    float dZ = nii_layer_r->pixdim[3];
    if (twodim == 1) {
        dZ = 1000 * dZ;
    }

    // ========================================================================
    // Fixing potential problems with different input datatypes
    nifti_image* nii_layer = copy_nifti_as_float32(nii_layer_r);
    float* nii_layer_data = static_cast<float*>(nii_layer->data);

    nifti_image* nii_landmark = copy_nifti_as_int32(nii_layer_r);
    int* nii_landmark_data = static_cast<int*>(nii_landmark->data);

    // Allocate necessary files
    nifti_image* Grow_x = copy_nifti_as_int32(nii_layer);
    nifti_image* Grow_y = copy_nifti_as_int32(nii_layer);
    nifti_image* Grow_z = copy_nifti_as_int32(nii_layer);

    int32_t *Grow_x_data = static_cast<int32_t*>(Grow_x->data);
    int32_t *Grow_y_data = static_cast<int32_t*>(Grow_y->data);
    int32_t *Grow_z_data = static_cast<int32_t*>(Grow_z->data);

    nifti_image* growfromCenter = copy_nifti_as_int32(nii_layer);
    nifti_image* growfromCenter_thick = copy_nifti_as_int32(nii_layer);
    nifti_image* growfromLeft = copy_nifti_as_int32(nii_layer);
    nifti_image* growfromRight = copy_nifti_as_int32(nii_layer);

    int32_t* growfromCenter_data = static_cast<int32_t*>(growfromCenter->data);
    int32_t* growfromCenter_thick_data = static_cast<int32_t*>(growfromCenter_thick->data);
    int32_t* growfromLeft_data = static_cast<int32_t*>(growfromLeft->data);
    int32_t* growfromRight_data = static_cast<int32_t*>(growfromRight->data);

    nifti_image* lateralCoord = copy_nifti_as_int32(nii_layer);
    nifti_image* inferioCoord = copy_nifti_as_int32(nii_layer);
    int32_t* lateralCoord_data = static_cast<int32_t*>(lateralCoord->data);
    int32_t* inferioCoord_data = static_cast<int32_t*>(inferioCoord->data);
    // ========================================================================

    // Finding number of layers
    int nr_layers = 0;
    for (int i = 0; i < nr_voxels; ++i) {
        if (*(nii_layer_data + i) > nr_layers) {
            nr_layers = *(nii_layer_data + i);
        }
    }
    cout << "  There are " << nr_layers << " layers." << endl;

    ///////////////////////////////
    // Prepare growing variables //
    ///////////////////////////////
    float x1g = 0., y1g = 0., z1g = 0.;

    float dist_min2 = 0.;
    float d = 0.;
    float d_p1 = 0.;

    int grow_vinc = 3;
    int grow_vinc_area = 1;
    int vinc_max = 250;

    // ========================================================================
    // Growing from Center
    // ========================================================================
    cout << "  Growing from center " << endl;

    // Defining seed at center landmark
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_y; ++iy) {
            for (int ix = 0; ix < size_x; ++ix) {
                int voxel_i = nxy * iz + nx * iy + ix;
                if (*(nii_landmark_data + voxel_i) == 1
                    && abs((int) (*(nii_layer_data + voxel_i) - nr_layers / 2)) < 2) {
                    *(growfromCenter_data + voxel_i) = 1.;
                    *(Grow_x_data + voxel_i) = ix;
                    *(Grow_y_data + voxel_i) = iy;
                    *(Grow_z_data + voxel_i) = iz;
                }
            }
        }
    }
    for (int grow_i = 1; grow_i < vinc_max; grow_i++) {
        for (int iz = 0; iz < size_z; ++iz) {
            for (int iy = 0; iy < size_x; ++iy) {
                for (int ix = 0; ix < size_y; ++ix) {
                    dist_min2 = 10000.;
                    x1g = 0;
                    y1g = 0;
                    z1g = 0;
                    int voxel_i = nxy * iz + nx * ix + iy;

                    if (abs((int) (*(nii_layer_data + voxel_i) - nr_layers/2)) < 2
                        && *(growfromCenter_data + voxel_i) == 0
                        && *(nii_landmark_data + voxel_i) < 2) {
                        // NOTE: Only grow into areas that are GM and that have not been grown into, yet.
                        // And it should stop as soon as it hits tie border.

                        int jz_start = max(0, iy - grow_vinc_area);
                        int jz_stop = min(iy + grow_vinc_area + 1, size_x);
                        int jy_start = max(0, ix - grow_vinc_area);
                        int jy_stop = min(ix + grow_vinc_area + 1, size_y);
                        int jx_start = max(0, iz - grow_vinc_area);
                        int jx_stop = min(iz + grow_vinc_area + 1, size_z);

                        for (int jz = jz_start; jz < jz_stop; ++jz) {
                            for (int jy = jy_start; jy < jz_stop; ++jy) {
                                for (int jx = jx_start; jx < jx_stop; ++jx) {
                                    d = dist((float)ix, (float)iy, (float)iz,
                                             (float)jx, (float)jy, (float)jz,
                                             dX, dY, dZ);

                                    int voxel_j = nxy * jz + nx * jy + jx;
                                    if (*(growfromCenter_data + voxel_j) == grow_i
                                        && *(nii_landmark_data + voxel_i) < 2) {
                                        if (d < dist_min2) {
                                            dist_min2 = d;
                                            x1g = jx;
                                            y1g = jy;
                                            z1g = jz;
                                            d_p1 = dist_min2;
                                        }
                                    }
                                }
                            }
                        }
                        if (dist_min2 < 1.7) {  // TODO(Renzo): I DONT REMEMBER WHY I NEED THIS ????
                            *(growfromCenter_data + voxel_i) = grow_i+1;
                            *(Grow_x_data + voxel_i) = *(Grow_x_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                            *(Grow_y_data + voxel_i) = *(Grow_y_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                            *(Grow_z_data + voxel_i) = *(Grow_z_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                        }
                    }
                }
            }
        }
    }
    save_output_nifti(fin_layer, "finding_leaks", growfromCenter, true);

    // ========================================================================
    // Growing from left
    // ========================================================================
    cout << "  Growing from left..." << endl;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_y; ++iy) {
            for (int ix = 0; ix < size_x; ++ix) {
                int voxel_i = nxy * iz + nx * iy + ix;
                *(Grow_x_data + voxel_i) = 0;
                *(Grow_y_data + voxel_i) = 0;
                *(Grow_z_data + voxel_i) = 0;
            }
        }
    }
    // Defining seed at center landmark
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_y; ++iy) {
            for (int ix = 0; ix < size_x; ++ix) {
                int voxel_i = nxy * iz + nx * iy + ix;

                if (*(nii_landmark_data + voxel_i) == 2
                    && abs((int) (*(nii_layer_data + voxel_i) - nr_layers / 2)) < 2) {
                    *(growfromLeft_data + voxel_i) = 1.;
                    *(Grow_x_data + voxel_i) = ix;
                    *(Grow_y_data + voxel_i) = iy;
                    *(Grow_z_data + voxel_i) = iz;
                }
            }
        }
    }
    for (int grow_i = 1; grow_i < vinc_max; grow_i++) {
        for (int iz = 0; iz < size_z; ++iz) {
            for (int iy = 0; iy < size_x; ++iy) {
                for (int ix = 0; ix < size_y; ++ix) {
                    dist_min2 = 10000.;
                    x1g = 0;
                    y1g = 0;
                    z1g = 0;
                    int voxel_i = nxy * iz + nx * ix + iy;
                    if (abs((int) (*(nii_layer_data + voxel_i) - nr_layers / 2)) < 2
                        && *(growfromLeft_data + voxel_i) == 0
                        && *(growfromCenter_data + voxel_i) != 0) {
                        // NOTE: Only grow into areas that are GM and that have not been grown into, yet ...
                        // And it should stop as soon as it hits the border.

                        jz_start = max(0, iz - grow_vinc);
                        jz_stop = min(iz + grow_vinc + 1, size_z);
                        jy_start = max(0, iy - grow_vinc);
                        jy_stop = min(iy + grow_vinc + 1, size_x);
                        jx_start = max(0, ix - grow_vinc);
                        jx_stop = min(ix + grow_vinc + 1, size_y);

                        for (int jy = jy_start; jy < jy_stop; ++jy) {
                            for (int jx = jx_start; jx < jx_stop; ++jx) {
                                for (int jz = jz_start; jz < jz_stop; ++jz) {
                                    d = dist((float)ix, (float)iy, (float)iz,
                                             (float)jx, (float)jy, (float)jz,
                                             dX, dY, dZ);

                                    int voxel_j = nxy * jz + nx * jy + jx;
                                    if (*(growfromLeft_data + voxel_j) == grow_i) {
                                        if (d < dist_min2) {
                                            dist_min2 = d;
                                            x1g = jx;
                                            y1g = jy;
                                            z1g = jz;
                                            d_p1 = dist_min2;
                                        }
                                    }
                                }
                            }
                        }
                        if (dist_min2 < 1.7) {  // TODO(@Renzo): ???? I DONT REMEMBER WHY I NEED THIS ????
                            *(growfromLeft_data + voxel_i) = grow_i + 1;
                            *(Grow_x_data + voxel_i) = *(Grow_x_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                            *(Grow_y_data + voxel_i) = *(Grow_y_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                            *(Grow_z_data + voxel_i) = *(Grow_z_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // Growing from Right
    // ========================================================================
    cout << "  Growing from right..." << endl;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_y; ++iy) {
            for (int ix = 0; ix < size_x; ++ix) {
                int voxel_i = nxy * iz + nx * iy + ix;
                *(Grow_x_data + voxel_i) = 0;
                *(Grow_y_data + voxel_i) = 0;
                *(Grow_z_data + voxel_i) = 0;
            }
        }
    }
    // Defining seed at center landmark
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_y; ++iy) {
            for (int ix = 0; ix < size_x; ++ix) {
                int voxel_i = nxy * iz + nx * iy + ix;
                if (*(nii_landmark_data + voxel_i) == 3
                    && abs((int) (*(nii_layer_data + voxel_i) - nr_layers / 2)) < 2) {
                    *(growfromRight_data + voxel_i) = 1.;
                    *(Grow_x_data + voxel_i) = ix;
                    *(Grow_y_data + voxel_i) = iy;
                    *(Grow_z_data + voxel_i) = iz;
                }
            }
        }
    }
    for (int grow_i = 1; grow_i < vinc_max; grow_i++) {
        for (int iz = 0; iz < size_z; ++iz) {
            for (int iy = 0; iy < size_x; ++iy) {
                for (int ix = 0; ix < size_y; ++ix) {
                    dist_min2 = 10000.;
                    x1g = 0;
                    y1g = 0;
                    z1g = 0;
                    int voxel_i = nxy * iz + nx * ix + iy;
                    if (abs((int) (*(nii_layer_data + voxel_i) - nr_layers / 2)) < 2
                        && *(growfromRight_data + voxel_i) == 0
                        && *(growfromCenter_data + voxel_i) != 0) {
                        // NOTE: Only grow into areas that are GM and that have not been gown into, yet...
                        // And it should stop as soon as it hits tie border

                        int jy_start = max(0, iy - grow_vinc);
                        int jy_stop = min(iy + grow_vinc + 1, size_x);
                        int jx_start = max(0, ix - grow_vinc);
                        int jx_stop = min(ix + grow_vinc + 1, size_y);
                        int jz_start = max(0, iz - grow_vinc);
                        int jz_stop = min(iz + grow_vinc + 1, size_z);

                        for (int jz = jz_start; jz < jz_stop; ++jz) {
                            for (int jy = jy_start; jy < jy_stop; ++jy) {
                                for (int jx = jx_start; jx < jx_stop; ++jx) {
                                    d = dist((float)ix, (float)iy, (float)iz,
                                             (float)jx, (float)jy, (float)jz,
                                             dX, dY, dZ);

                                    int voxel_j = nxy * jz + nx * jy + jx;
                                    if (*(growfromRight_data + voxel_j) == grow_i) {
                                        if (d < dist_min2) {
                                            dist_min2 = d;
                                            x1g = jx;
                                            y1g = jy;
                                            z1g = jz;
                                            d_p1 = dist_min2;
                                        }
                                    }
                                }
                            }
                        }
                        if (dist_min2 < 1.7) {  // TODO(@Renzo): I DONT REMEMBER WHY I NEED THIS ????
                            *(growfromRight_data + voxel_i) = grow_i + 1;
                            *(Grow_x_data + voxel_i) = *(Grow_x_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                            *(Grow_y_data + voxel_i) = *(Grow_y_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                            *(Grow_z_data + voxel_i) = *(Grow_z_data + nxy * (int)z1g + nx * (int)x1g + (int)y1g);
                        }
                    }
                }
            }
        }
    }
    save_output_nifti(fin_layer, "grow_from_right", growfromRight, true);

    // ========================================================================
    // Get normalized coordinate system
    // ========================================================================
    cout << "  Getting normalized coordinate system..." << endl;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                int voxel_i = nxy * iz + nx * ix + iy;
                if (*(growfromCenter_data + voxel_i) > 0) {

                    float grow_r = static_cast<float>(*(growfromRight_data + voxel_i));
                    float grow_l = static_cast<float>(*(growfromLeft_data + voxel_i));

                    *(lateralCoord_data + voxel_i) =
                        static_cast<int>(200 * grow_r / (grow_r + grow_l));
                    // *(growfromCenter_thick_data + voxel_i) = 1;
                }
            }
        }
    }
    save_output_nifti(fin_layer, "lateral_cord", lateralCoord, true);

    //////////////////////////
    // Sparse visualisation //
    //////////////////////////
    // cout << " sparse visualisation " << endl;
    // cout << " visualisation " << endl;
    // for (int iz = 0; iz < size_z; ++iz) {
    //     for (int iy = 0; iy < size_x; ++iy) {
    //         for (int ix = 0; ix < size_y; ++ix) {
    //             if (iy % 4 == 0 || iz % 4 == 0) {
    //                 *(lateralCoord_data + nxy * iz + nx * ix + iy) =
    //                     *(lateralCoord_data + nxy * iz + nx * ix + iy);
    //             } else {
    //                 *(lateralCoord_data + nxy * iz + nx * ix + iy) = 0;
    //             }
    //         }
    //     }
    // }
    // cout << "  Not Segmentation fault until here 381" << endl;

    ////////////////////
    // Smooth columns //
    ////////////////////
    cout << "  Smoothing in middle layer..." << endl;

    nifti_image* nii_smooth = copy_nifti_as_float32(nii_layer);
    nifti_image* nii_gaussw = copy_nifti_as_float32(nii_layer);
    float* nii_smooth_data = static_cast<float*>(nii_smooth->data);
    float* nii_gaussw_data = static_cast<float*>(nii_gaussw->data);

    int FWHM_val = 1;
    int vinc_sm = max(1., 2. * FWHM_val / dX);  // Ignore if voxel is too far
    d = 0.;
    cout << "  vinc_sm " << vinc_sm<< endl;
    cout << "  FWHM_val " << FWHM_val<< endl;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                *(nii_gaussw_data + nxy * iz + nx * ix + iy) = 0;
                // *(nii_smooth_data + nxy * iz + nx * ix + iy) = 0;

                if (*(lateralCoord_data + nxy * iz + nx * ix + iy)  > 0) {
                    for (int iz_i = max(0, iz - vinc_sm); iz_i < min(iz + vinc_sm + 1, size_z); ++iz_i) {
                        for (int iy_i = max(0, iy - vinc_sm); iy_i < min(iy + vinc_sm + 1, size_x); ++iy_i) {
                            for (int ix_i = max(0, ix - vinc_sm); ix_i<min(ix + vinc_sm + 1, size_y); ++ix_i) {
                                if (*(lateralCoord_data + nxy * iz_i + nx * ix_i + iy_i)  > 0) {
                                    dist_i = dist((float)ix, (float)iy, (float)iz, (float)ix_i, (float)iy_i, (float)iz_i, dX, dY, dZ);
                                    // cout << "debug 4 " << gaus(dist_i, FWHM_val) << endl;
                                    // cout << "debug 5 " << dist_i << endl;
                                    // if (*(nim_input_data + nxy * iz + nx * ix + iy) == 3) cout << "debug  4b " << endl;
                                    // dummy = *(layer_data + nxy * iz_i + nx * ix_i + iy_i);
                                    *(nii_smooth_data + nxy * iz + nx * ix + iy) = *(nii_smooth_data + nxy * iz + nx * ix + iy) + *(lateralCoord_data + nxy * iz_i + nx * ix_i + iy_i) * gaus(dist_i, FWHM_val);
                                    *(nii_gaussw_data + nxy * iz + nx * ix + iy) = *(nii_gaussw_data + nxy * iz + nx * ix + iy) + gaus(dist_i, FWHM_val);
                                }
                            }
                        }
                    }
                    if (*(nii_gaussw_data + nxy * iz + nx * ix + iy) > 0) {
                        *(nii_smooth_data + nxy * iz + nx * ix + iy) = *(nii_smooth_data + nxy * iz + nx * ix + iy) / *(nii_gaussw_data + nxy * iz + nx * ix + iy);
                    }
                }
                //if (*(nii_layer_r_data + nxy * iz + nx * ix + iy) <= 0) *(nii_smooth_data + nxy*iz + nx * ix + iy) = *(lateralCoord_data + nxy*iz + nx*ix + iy);
            }
        }
    }
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                if (*(growfromCenter_data + nxy * iz + nx * ix + iy) > 0) {
                    *(lateralCoord_data + nxy * iz + nx * ix + iy) = (int) *(nii_smooth_data + nxy * iz + nx * ix + iy);
                }
            }
        }
    }
    // const char *fout_10 = "lateral_cord_nii_smooth.nii";
    // if (nifti_set_filenames(lateralCoord, fout_10, 1, 1)){
    //     return 1;
    // }
    // nifti_image_write(lateralCoord);

    /////////////////////////////////////
    // Extending columns across layers //
    /////////////////////////////////////
    cout << "  Extending columns across layers..." << endl;

    nifti_image* hairy_brain = copy_nifti_as_int32(nii_layer);
    int32_t* hairy_brain_data = static_cast<int32_t*>(hairy_brain->data);

    nifti_image* hairy_brain_dist = copy_nifti_as_float32(nii_layer);
    float* hairy_brain_dist_data = static_cast<float*>(hairy_brain_dist->data);

    dist_min2 = 10000.;  // This is an upper limit of the cortical thickness
    int vinc_thickness = 13;  // Closest middle layer area algorithm looks for
    int cloasest_coord = 0.;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                *(hairy_brain_dist_data + nxy * iz + nx * ix + iy) = dist_min2;
            }
        }
    }

    // *(nii_layer_data + nxy * iz + nx * ix + iy) > 0

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                dist_min2 = 10000.;
                if (*(lateralCoord_data + nxy*iz + nx*ix + iy) > 0) {
                    // for (int iz_i = max(0, iz-vinc_thickness + 1); iz_i < min(iz + vinc_thickness + 1, size_z); ++iz_i) {
                    int iz_i = iz;
                    for (int iy_i = max(0, iy-vinc_thickness+1); iy_i<min(iy+vinc_thickness+1, size_x); ++iy_i) {
                        for (int ix_i = max(0, ix-vinc_thickness+1); ix_i<min(ix+vinc_thickness+1, size_y); ++ix_i) {
                            if (*(lateralCoord_data + nxy*iz_i + nx*ix_i + iy_i) == 0  && *(nii_layer_data + nxy*iz + nx*ix + iy) > 0 && dist((float)ix, (float)iy, (float)iz, (float)ix_i, (float)iy_i, (float)iz_i, dX, dY, dZ) < vinc_thickness) {
                                dist_i = dist((float)ix, (float)iy, (float)iz, (float)ix_i, (float)iy_i, (float)iz_i, dX, dY, dZ);

                                if (dist_i < *(hairy_brain_dist_data + nxy * iz_i + nx * ix_i + iy_i) && *(nii_layer_data + nxy * iz + nx * ix + iy) > 0) {
                                    *(hairy_brain_dist_data + nxy * iz_i + nx * ix_i + iy_i) = dist_i;
                                    *(hairy_brain_data + nxy * iz_i + nx * ix_i + iy_i) = *(lateralCoord_data + nxy * iz + nx * ix + iy);
                                }
                            }
                            if (*(lateralCoord_data + nxy * iz_i + nx * ix_i + iy_i) != 0) {
                                *(hairy_brain_data + nxy * iz_i + nx * ix_i + iy_i) = *(lateralCoord_data + nxy * iz_i + nx * ix_i + iy_i);
                            }
                        }
                    }
                    // }
                    // *(hairy_brain_data + nxy * iz + nx * ix + iy) = cloasest_coord;
                }
            }
        }
    }

    ////////////////////
    // Smooth columns //
    ////////////////////
    cout << "  Smoothing hairy brain again... " << endl;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                *(nii_smooth_data + nxy * iz + nx *ix + iy) = 0;
            }
        }
    }

    FWHM_val = 1;
    vinc_sm = max(1., 2. * FWHM_val / dX);
    cout << "  vinc_sm " << vinc_sm << endl;
    cout << "  FWHM_val " << FWHM_val << endl;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y -0; ++ix) {
                *(nii_gaussw_data + nxy * iz + nx * ix + iy) = 0;
                // *(nii_smooth_data + nxy * iz + nx * ix + iy) = 0;

                if (*(hairy_brain_data + nxy * iz + nx * ix + iy) > 0) {
                    int nr_layers_i = *(nii_layer_data + nxy * iz + nx * ix + iy);

                    //for (int iz_i = max(0, iz - vinc_sm); iz_i < min(iz + vinc_sm + 1, size_y); ++iz_i) {
                    int iz_i = iz;
                    for (int iy_i = max(0, iy - vinc_sm); iy_i < min(iy + vinc_sm + 1, size_x); ++iy_i) {
                        for (int ix_i = max(0, ix - vinc_sm); ix_i < min(ix + vinc_sm + 1, size_y); ++ix_i) {
                            if (abs((int) *(nii_layer_data + nxy * iz_i + nx * ix_i + iy_i) - nr_layers_i) < 2 && *(hairy_brain_data + nxy * iz_i + nx * ix_i + iy_i) > 0) {
                                dist_i = dist((float)ix, (float)iy, (float)iz, (float)ix_i, (float)iy_i, (float)iz_i, dX, dY, dZ);
                                // cout << "debug  4 " << gaus(dist_i, FWHM_val) << endl;
                                // cout << "debug  5 " << dist_i << endl;
                                // if (*(nim_input_data + nxy*iz + nx*ix + iy) == 3) cout << "debug  4b " << endl;
                                // dummy = *(layer_data + nxy*iz_i + nx*ix_i + iy_i);
                                *(nii_smooth_data + nxy * iz + nx * ix + iy) = *(nii_smooth_data + nxy * iz + nx * ix + iy) + *(hairy_brain_data + nxy * iz_i + nx * ix_i + iy_i) * gaus(dist_i, FWHM_val);
                                *(nii_gaussw_data + nxy * iz + nx * ix + iy) = *(nii_gaussw_data + nxy * iz + nx * ix + iy) + gaus(dist_i, FWHM_val);
                            }
                        }
                        // }
                    }
                    if (*(nii_gaussw_data + nxy * iz + nx * ix + iy) > 0) {
                        *(nii_smooth_data + nxy * iz + nx * ix + iy) = *(nii_smooth_data + nxy * iz + nx * ix + iy) / *(nii_gaussw_data + nxy * iz + nx * ix + iy);
                    }
                }
            }
        }
    }
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                if (*(hairy_brain_data + nxy * iz + nx * ix + iy) > 0) {
                    *(hairy_brain_data + nxy * iz + nx * ix + iy) = (int) *(nii_smooth_data + nxy * iz + nx * ix + iy);
                }
            }
        }
    }

    //////////////////////////
    // Sparse visualisation //
    //////////////////////////
    cout << "  Visualisation..." << endl;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                if ((int) *(nii_smooth_data + nxy * iz + nx * ix + iy) % 10 == 0 && iz % 10 == 0) {
                    *(growfromRight_data + nxy * iz + nx * ix + iy) = *(nii_smooth_data + nxy * iz + nx *ix + iy);
                }
                else *(growfromRight_data + nxy * iz + nx * ix + iy) = 0;
            }
        }
    }

    //////////////////////////////////
    // Growing from as thick cortex //
    //////////////////////////////////
    cout << "  Growing from center with thick cortex..." << endl;
    cout << "  Jiajia Option is on." << endl;

    int Jiajia_otion = 1;
    int grow_vinc_thick = 1;
    int vinc_max_thick = 17;
    int grow_vinc_area_thick = 1;

    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                // Defining seed at center landmark
                if (*(growfromCenter_data + nxy * iz + nx * ix + iy) > 0) {
                    *(growfromCenter_thick_data + nxy * iz + nx * ix + iy) = 1;
                }
            }
        }
    }
    if (jiajiaoption == 1)  {
        for (int grow_i = 1; grow_i < vinc_max_thick; grow_i++) {
            for (int iz = 0; iz < size_z; ++iz) {
                for (int iy = 0; iy < size_x; ++iy) {
                    for (int ix = 0; ix < size_y; ++ix) {
                        if ( *(nii_layer_data + nxy * iz + nx * ix + iy) > 0 && *(nii_layer_data + nxy * iz + nx * ix + iy) <= nr_layers && *(growfromCenter_thick_data + nxy * iz + nx * ix + iy) == grow_i && *(hairy_brain_data + nxy * iz + nx *ix + iy) > 0) {
                            // Note: Only grow into areas that are GM and that have not been grown into, yet...
                            // And it should stop as soon as it hits the border
                            int iz_i = iz;
                            // int stopme = 0;
                            for (int ix_i = max(0, ix - grow_vinc_area_thick); ix_i < min(ix + grow_vinc_area_thick + 1, size_y); ++ix_i) {
                                for (int iy_i = max(0, iy - grow_vinc_area_thick); iy_i < min(iy + grow_vinc_area_thick + 1, size_x); ++iy_i) {
                                    dist_i = dist((float)ix, (float)iy, (float)iz, (float)ix_i, (float)iy_i, (float)iz_i, dX, dY, dZ);
                                    // if (*(nii_layer_data + nxy * iz_i + nx * ix_i + iy_i) == nr_layers) stopme = 1;
                                    if (dist_i <= (dY + dX) / 2. && *(growfromCenter_thick_data + nxy * iz_i + nx * ix_i + iy_i) == 0 && *(nii_layer_data + nxy * iz_i + nx * ix_i + iy_i) <= nr_layers && *(nii_layer_data + nxy * iz_i + nx * ix_i + iy_i) > 0 && *(hairy_brain_data + nxy * iz + nx * ix + iy) > 0) {
                                        *(growfromCenter_thick_data + nxy * iz_i + nx * ix_i + iy_i) = grow_i+1;
                                    }
                                }
                            }
                            // cout << " ix   " << ix << " iy   " << iy << "    " << *(WMkoord0_data + nxy*islice + nx*(int)x1g + (int)y1g)<< endl;
                        }
                    }
                }
            }
        }
    }
    if (jiajiaoption != 1) {
        for (int grow_i = 1; grow_i < vinc_max_thick; grow_i++) {
            for (int iz = 0; iz < size_z; ++iz) {
                for (int iy = 0; iy <size_x; ++iy) {
                    for (int ix = 0; ix < size_y; ++ix) {
                        if ( *(nii_layer_data + nxy*iz + nx*ix + iy) > 0 && *(nii_layer_data + nxy*iz + nx*ix + iy) < nr_layers && *(growfromCenter_thick_data + nxy*iz + nx*ix + iy) == grow_i && *(hairy_brain_data + nxy*iz + nx*ix + iy) > 0) {
                            // Note: Only grow into areas that are GM and that have not been gown into, yet...
                            // And it should stop as soon as it hits the border.
                            int iz_i = iz;
                            // int stopme = 0;
                            for (int ix_i = max(0, ix - grow_vinc_area_thick); ix_i < min(ix + grow_vinc_area_thick + 1, size_y); ++ix_i) {
                                for (int iy_i = max(0, iy - grow_vinc_area_thick); iy_i<min(iy + grow_vinc_area_thick + 1, size_x); ++iy_i) {
                                    dist_i = dist((float)ix, (float)iy, (float)iz, (float)ix_i, (float)iy_i, (float)iz_i, dX, dY, dZ);
                                    // if (*(nii_layer_data + nxy * iz_i + nx * ix_i + iy_i) == nr_layers) stopme = 1;
                                    if (dist_i <= (dY + dX) / 2. && *(growfromCenter_thick_data + nxy * iz_i + nx * ix_i + iy_i) == 0 && *(nii_layer_data + nxy * iz_i + nx * ix_i + iy_i) < nr_layers && *(nii_layer_data + nxy * iz_i + nx * ix_i + iy_i) > 0 && *(hairy_brain_data + nxy * iz + nx * ix + iy) > 0) {
                                        *(growfromCenter_thick_data + nxy * iz_i + nx * ix_i + iy_i) = grow_i + 1;
                                    }
                                }
                            }
                            // cout << " ix   " << ix << " iy   " << iy << "    " << *(WMkoord0_data + nxy*islice + nx*(int)x1g + (int)y1g)<< endl;
                        }
                    }
                }
            }
        }
    }
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                if ((*(nii_layer_data + nxy * iz + nx * ix + iy) == nr_layers - 1 || *(nii_layer_data + nxy * iz + nx * ix + iy) == nr_layers - 2) && *(growfromCenter_thick_data + nxy * iz + nx * ix + iy) > 0) {
                    // only grow into areas that are GM and that have not been gown into, yet .... and it should stop as soon as it hits tie border
                    for (int iy_i = max(0, iy - grow_vinc_area_thick); iy_i < min(iy + grow_vinc_area_thick + 1, size_x); ++iy_i) {
                        for (int ix_i = max(0, ix - grow_vinc_area_thick); ix_i < min(ix + grow_vinc_area_thick + 1, size_y); ++ix_i) {
                            int iz_i = iz;
                            dist_i = dist((float)ix, (float)iy, (float)iz, (float)ix_i, (float)iy_i, (float)iz_i, dX, dY, dZ);

                            if (dist_i <= (dY + dX) / 2. && *(nii_landmark_data + nxy * iz_i + nx * ix_i + iy_i) > 0) {
                                *(growfromCenter_thick_data + nxy * iz_i + nx * ix_i + iy_i) = *(growfromCenter_thick_data + nxy * iz + nx * ix + iy);
                            }
                        }
                    }
                }
            }
        }
    }

    /////////////////////////////
    // Clean up hairy brain /////
    /////////////////////////////
    cout << "  Cleaning up hairy brain..." << endl;
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                if (*(growfromCenter_thick_data + nxy * iz + nx * ix + iy) == 0 || *(hairy_brain_data + nxy * iz + nx * ix + iy) == 0) {
                    *(growfromRight_data + nxy*iz + nx*ix + iy) = 0;
                    *(hairy_brain_data + nxy*iz + nx*ix + iy) = 0;
                }
            }
        }
    }

    //////////////////////////////
    // Change number of columns //
    //////////////////////////////
    cout << "  Number of columns." << endl;
    int max_columns = 0;
    int min_columns = 100000000;
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                if (*(hairy_brain_data + nxy * iz + nx * ix + iy) >  0) {
                    if ((int) *(hairy_brain_data + nxy * iz + nx *ix + iy) > max_columns) max_columns = (int) *(hairy_brain_data + nxy * iz + nx * ix + iy);
                    if ((int) *(hairy_brain_data + nxy * iz + nx *ix + iy) < min_columns) min_columns = (int) *(hairy_brain_data + nxy * iz + nx * ix + iy);
                }
            }
        }
    }
    cout << "  Max = " << max_columns << " | Min = " << min_columns << endl;
    for (int iz = 0; iz < size_z; ++iz) {
        for (int iy = 0; iy < size_x; ++iy) {
            for (int ix = 0; ix < size_y; ++ix) {
                if (*(hairy_brain_data + nxy * iz + nx * ix + iy) >  0) {
                    *(hairy_brain_data + nxy * iz + nx * ix + iy) = (*(hairy_brain_data + nxy * iz + nx * ix + iy) - (short)min_columns) * (short)jiajiavinc_max / (short)(max_columns - min_columns);
                }
            }
        }
    }

    save_output_nifti(fin_layer, "column_coordinates", hairy_brain, true);

    cout << "  Finished." << endl;
    return 0;
}
