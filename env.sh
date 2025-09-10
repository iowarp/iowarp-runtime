module load cte-hermes-shm
scspkg build profile m=cmake path=.env.cmake
scspkg build profile m=dotenv path=.env
scspkg build profile m=cmake path=test/unit/external-chimod/.env.cmake
