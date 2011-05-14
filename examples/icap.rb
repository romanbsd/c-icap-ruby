module Icap
  module Codes
    NOT_READY = 0
    DONE = 1
    CONTINUE = 100
    ALLOW204 = 204
    ERROR = -1
  end

  class AbstractAdapter
    attr_reader :reqheaders, :respheaders

    def initialize(reqheaders, respheaders)
      @reqheaders, @respheaders = reqheaders, respheaders
    end

    # End of data handler, called when the client has no more data to send
    #
    # @abstract
    def end_of_data
      raise NotImplementedError
    end

    # Read data handler, reads data from the client
    #
    # @abstract
    # @param [String] data Data chunk from the client
    def read_data(data)
      raise NotImplementedError
    end

    # Write data handler, sends data to client
    #
    # @abstract
    # @param [Fixnum] length
    # @return [String] Data chunk
    def write_data(length)
      raise NotImplementedError
    end

    # Optional methods
    #

    # Preview handler
    #
    # @abstract
    # @param [String] data The preview data
    # @return [Fixnum] One of Icap::Codes
    ##
    # :method preview(data)

    # Allow asynchronous IO?
    #
    # @abstract
    # @return [Boolean]
    ##
    # :method async?

  end
end
