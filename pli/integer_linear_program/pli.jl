#==========================================================================
# Implementation of an integer linear program for 2-Strong Roman Domination
# Author: Atílio Gomes Luiz
# Data: October, 9th 2026
==========================================================================#

# Importing the necessary packages
using DelimitedFiles
using JuMP, CPLEX
using Graphs
using Dates
using MathOptInterface

const MOI = MathOptInterface

include("../greedy/Greedy_algorithm.jl")

# Defining a constant that controls debug messages
const DEBUG = true # false to deactivate

# The @debug macro for wrapping any code block
macro debug(block)
    return :(if DEBUG
                $(esc(block))
            end)
end


"""
    read_simple_graph(filepath::AbstractString) -> SimpleGraph

Reads a .txt file containing an adjacency matrix with values separated
by whitespace (one line per vertex) and returns a `SimpleGraph`.

The matrix can contain either integer or floating-point values, e.g.:

    0 1 1 0
    1 0 1 1
    1 1 0 0
    0 1 0 0

or

    0.0 1.0 1.0 0.0
    1.0 0.0 1.0 1.0
    1.0 1.0 0.0 0.0
    0.0 1.0 0.0 0.0

Any nonzero value is interpreted as an edge.

The diagonal is ignored, even if it contains nonzero values.
"""
function read_simple_graph(filepath::AbstractString)::SimpleGraph

    # Read all non-empty lines
    linhas = readlines(filepath)
    linhas = filter(l -> !isempty(strip(l)), linhas)

    # Number of vertices
    n = length(linhas)

    if n == 0
        error("The adjacency matrix is empty: $filepath")
    end

    # Create an empty simple graph
    g = SimpleGraph(n)

    for (i, linha) in enumerate(linhas)

        # Read values as Float64
        valores = parse.(Float64, split(strip(linha)))

        # Check whether the matrix is square
        if length(valores) != n
            error(
                "Line $i has $(length(valores)) values, " *
                "but $n values were expected " *
                "(matrix must be $n x $n): $filepath"
            )
        end

        # Only inspect the upper triangular part.
        # This avoids adding the same undirected edge twice
        # and ignores the diagonal.
        for j in (i+1):n

            # Any nonzero value represents an edge
            if !iszero(valores[j])
                add_edge!(g, i, j)
            end
        end
    end

    return g
end


#=
    This function takes as input the simple graph, the maximum time (in minutes)
    allowed for the solver to run on this graph, and a viable solution.
    If the solver does not find the optimal solution within this time,
    the best solution found will be returned.
=#
function twoStrongRoman(
    graph::SimpleGraph,
    timelimitMinutes::Int,
    color::Dict{Int,Int}
)

    # Preparing the optimization model
    model = Model(CPLEX.Optimizer)

    set_optimizer_attribute(model, MOI.Silent(), true)

    # Setting the maximum time (in seconds)
    set_optimizer_attribute(
        model,
        MOI.TimeLimitSec(),
        timelimitMinutes * 60
    )

    # ------------------------------------------------------------------------
    # Defining decision variables
    @variable(model, z[v in vertices(graph), k in 0:3], Bin)
    @variable(model, a[u in vertices(graph), v in neighbors(graph, u)], Bin)
    @variable(model, q[u in vertices(graph), k in 0:2], Bin)

    # -------------------------------------------------------------------------
    # Constraint R1: each vertex receives exactly one label
    for v in vertices(graph)
        @constraint(
            model,
            z[v,0] + z[v,1] + z[v,2] + z[v,3] == 1
        )
    end

    for u in vertices(graph)

        deg_u = degree(graph, u)

        if deg_u == 0

            # Restriction R1.1
            # Isolated vertex must have label == 1
            @constraint(model, z[u,1] == 1)

            continue
        end

        # Calculates the number of neighbors of u that are protectors
        protector_sum(u) =
            sum(z[w,2] + z[w,3] for w in neighbors(graph, u))

        # Constraint R2
        @constraint(
            model,
            protector_sum(u) >= z[u,0]
        )

        # Constraint R3
        @constraint(
            model,
            sum(a[w,u] for w in neighbors(graph, u)) <= z[u,2]
        )

        # Constraints R4.1-R4.6
        @constraint(
            model,
            q[u,0] + q[u,1] + q[u,2] == 1
        )

        @constraint(
            model,
            protector_sum(u) <= deg_u * (1 - q[u,0])
        )

        @constraint(
            model,
            protector_sum(u) <= 1 + deg_u * (1 - q[u,1])
        )

        @constraint(
            model,
            protector_sum(u) >= 1 - deg_u * (1 - q[u,1])
        )

        @constraint(
            model,
            protector_sum(u) >= 2 * q[u,2]
        )

        @constraint(
            model,
            protector_sum(u) <= 1 + (deg_u - 1) * q[u,2]
        )
    end

    # Constraints R5-R8
    for u in vertices(graph)
        for v in neighbors(graph, u)

            @constraint(
                model,
                a[u,v] <= z[u,0]
            )

            @constraint(
                model,
                a[u,v] <= z[v,2]
            )

            @constraint(
                model,
                a[u,v] <= q[u,1]
            )

            @constraint(
                model,
                a[u,v] >= z[u,0] + z[v,2] + q[u,1] - 2
            )
        end
    end

    # -------------------------------------------------------------------------
    # Objective function
    @objective(
        model,
        Min,
        sum(
            z[v,1] + 2*z[v,2] + 3*z[v,3]
            for v in vertices(graph)
        )
    )

    # -------------------------------------------------------------------------
    # Model information
    @debug begin

        num_vars = JuMP.num_variables(model)

        println(
            "Number of variables of the model: ",
            num_vars
        )

        num_cons = JuMP.num_constraints(
            model;
            count_variable_in_set_constraints = false
        )

        println(
            "Number of restrictions of the model: ",
            num_cons
        )
    end

    # -------------------------------------------------------------------------
    # WARM START
    # -------------------------------------------------------------------------

    # Initial values for z[v,k]
    for v in vertices(graph)

        c = color[v]

        for k in 0:3

            if k == c
                set_start_value(z[v,k], 1)
            else
                set_start_value(z[v,k], 0)
            end

        end
    end

    # -------------------------------------------------------------------------
    # Protector sets
    # -------------------------------------------------------------------------

    protector_set = Dict(
        i => Set{Int}()
        for i in vertices(graph)
    )

    for v in vertices(graph)

        if color[v] > 0

            for w in neighbors(graph, v)

                push!(
                    get!(
                        protector_set,
                        w,
                        Set{Int}()
                    ),
                    v
                )

            end
        end
    end

    # -------------------------------------------------------------------------
    # Initial values for a[u,w]
    # -------------------------------------------------------------------------

    for u in vertices(graph)

        for w in neighbors(graph, u)

            if (
                color[u] == 0 &&
                color[w] == 2 &&
                length(protector_set[u]) == 1
            )

                set_start_value(a[u,w], 1)

            else

                set_start_value(a[u,w], 0)

            end
        end
    end

    # -------------------------------------------------------------------------
    # Initial values for q[u,k]
    # -------------------------------------------------------------------------

    for u in vertices(graph)

        if length(protector_set[u]) == 0

            set_start_value(q[u,0], 1)
            set_start_value(q[u,1], 0)
            set_start_value(q[u,2], 0)

        elseif length(protector_set[u]) == 1

            set_start_value(q[u,0], 0)
            set_start_value(q[u,1], 1)
            set_start_value(q[u,2], 0)

        else

            set_start_value(q[u,0], 0)
            set_start_value(q[u,1], 0)
            set_start_value(q[u,2], 1)

        end
    end

    # -------------------------------------------------------------------------
    # Solve
    # -------------------------------------------------------------------------

    start_time = now()

    JuMP.optimize!(model)

    end_time = now()

    @debug begin

        println(
            stderr,
            "Termination status: ",
            termination_status(model)
        )

        println(
            stderr,
            "Primal status: ",
            primal_status(model)
        )

        println(
            stderr,
            "Dual status: ",
            dual_status(model)
        )
    end

    status = termination_status(model)

    if has_values(model)

        obj = JuMP.objective_value(model)

        z_star = JuMP.value.(z)

    else

        println(
            stderr,
            "Error: solution not found"
        )

        exit(1)
    end

    return z_star, obj, Dates.value(end_time - start_time), status
end


# ============================================================================
# Main Function
# ============================================================================

function main()

    # -------------------------------------------------------------------------
    # Command-line arguments
    #
    # ARGS[1] = input file
    # ARGS[2] = output directory
    # -------------------------------------------------------------------------

    if length(ARGS) < 2

        println(
            stderr,
            "Usage: julia linear_program.jl <arquivo.txt> <output_dir>"
        )

        exit(1)
    end

    # Input file
    filepath = abspath(ARGS[1])

    # Output directory
    output_dir = abspath(ARGS[2])

    # -------------------------------------------------------------------------
    # Check input file
    # -------------------------------------------------------------------------

    if !isfile(filepath)

        println(
            stderr,
            "Error: input file not found -> ",
            filepath
        )

        exit(1)
    end

    # -------------------------------------------------------------------------
    # Create output directory if necessary
    # -------------------------------------------------------------------------

    mkpath(output_dir)

    # -------------------------------------------------------------------------
    # Extract filename
    # -------------------------------------------------------------------------

    filename, ext = splitext(basename(filepath))

    # -------------------------------------------------------------------------
    # Read graph
    # -------------------------------------------------------------------------

    g = read_simple_graph(filepath)

    n = nv(g)
    m = ne(g)

    # Avoid division by zero for n = 1
    if n > 1
        density = 2.0 * m / (n * (n - 1))
    else
        density = 0.0
    end

    @debug begin

        println("Input file = $filepath")
        println("Output dir = $output_dir")
        println("n = $n, m = $m, density = $density")

    end

    # -------------------------------------------------------------------------
    # Degree information
    # -------------------------------------------------------------------------

    @debug begin

        for v in vertices(g)

            if degree(g, v) == 0

                println(
                    "vértice de grau zero: ",
                    v
                )

            end
        end
    end

    max_degree = maximum(
        degree(g, v)
        for v in vertices(g)
    )

    min_degree = minimum(
        degree(g, v)
        for v in vertices(g)
    )

    # -------------------------------------------------------------------------
    # Solve
    # -------------------------------------------------------------------------

    TIME_LIMIT_MINUTES = 10

    color_map = greedy_solution(g)

    z, opt, elapsed_time, status =
        twoStrongRoman(
            g,
            TIME_LIMIT_MINUTES,
            color_map
        )

    # -------------------------------------------------------------------------
    # Debug solution
    # -------------------------------------------------------------------------

    @debug begin

        println("opt = ", opt)

        for v in vertices(g)

            println(
                "label($v) = ",
                z[v,1] +
                2*z[v,2] +
                3*z[v,3]
            )

        end
    end

    # -------------------------------------------------------------------------
    # Output
    # -------------------------------------------------------------------------

    result_file_path = joinpath(
        output_dir,
        filename * ".csv"
    )

    open(result_file_path, "w") do io

        # Header
        println(
            io,
            "G,|V|,|E|,density,maxDegree,minDegree,weight,time(milliseconds),status"
        )

        # Results
        println(
            io,
            "$(filename),",
            "$(nv(g)),",
            "$(ne(g)),",
            "$(density),",
            "$(max_degree),",
            "$(min_degree),",
            "$(opt),",
            "$(elapsed_time),",
            "$(status)"
        )

    end

    println(
        stdout,
        "Results saved in: ",
        result_file_path
    )
end


# ============================================================================
# Program entry point
# ============================================================================

if abspath(PROGRAM_FILE) == @__FILE__
    main()
end