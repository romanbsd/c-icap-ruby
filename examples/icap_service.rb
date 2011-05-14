require 'icap'
class IcapService < Icap::AbstractAdapter

  def initialize(reqheaders, respheaders)
    super
    @eod = false
    @allow_204 = false
    # Only modify text/html (just a precaution, should be in config)
    unless @respheaders.find {|h| h.include?('text/html')}
      @allow_204 = true
    end
    @httpresponse = ''.force_encoding('ASCII-8BIT')
  end

  def end_of_data
    @eod = true
  end

  def respheaders
    transform!
    @respheaders
  end

  def preview(data)
    if @allow_204
      return Icap::Codes::ALLOW204
    end
    read_data(data)
    return Icap::Codes::CONTINUE
  end

  # Buffer whole request body
  def read_data(data)
    @httpresponse << data
  end

  # Output result only after whole body was buffered
  def write_data(length)
    return '' unless @eod
    transform!
    return @httpresponse.slice!(0, length)
  end

  private

  def transform!
    unless @done
      @httpresponse, @respheaders = Transform.new(@httpresponse, @respheaders, @reqheaders).result
      @done = true
    end
  rescue Exception => e
    puts "Exception in transform: #{e.message} #{e.backtrace.join("\n")}"
  end

end
