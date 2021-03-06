package com.tck.player


import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import android.widget.SeekBar
import com.tck.player.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    companion object {
        const val TAG = "tck6666"
    }

    private lateinit var binding: ActivityMainBinding

    private var totalTime: Int = 0

    private lateinit var myMusicPlayer: MyMusicPlayer
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.setFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        )
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        //3.2.2.1 23
        myMusicPlayer = MyMusicPlayer()

        myMusicPlayer.setOnPreparedListener(object : OnPlayerListenerAdapter() {
            override fun onPrepare() {
                super.onPrepare()
                Log.d(TAG, "onPrepare")
                myMusicPlayer.start()
            }

            override fun onLoad(load: Boolean) {
                super.onLoad(load)
                if (load) {
                    Log.d(TAG, "加载中...")
                } else {
                    Log.d(TAG, "播放中...")
                }
            }

            override fun onPause() {
                super.onPause()
                Log.d(TAG, "暂停中...")
            }

            override fun onResume() {
                super.onResume()
                Log.d(TAG, "播放中...")
            }

            override fun onTimeInfo(currentTime: Int, totalTime: Int) {
                super.onTimeInfo(currentTime, totalTime)
                this@MainActivity.totalTime = totalTime
                Log.d(TAG, "currentTime:$currentTime,totalTime:$totalTime")
                runOnUiThread {
                    binding.pbPlayProgress.progress =
                        (currentTime.toFloat() / totalTime.toFloat() * 100).toInt()
                    binding.tvPlayDuration.text = TimeUtil.secondsToStr(totalTime)
                    binding.tvPlayCurrentProgress.text =
                        TimeUtil.secondsToStr(currentTime)
                }
            }
        })
        binding.btnStartPlay.setOnClickListener {
            open()
        }

        binding.btnPausePlay.setOnClickListener {
            pause()
        }

        binding.btnResumePlay.setOnClickListener {
            resume()
        }

        binding.btnStopPlay.setOnClickListener {
            myMusicPlayer.stop()
        }

        binding.sbAudio.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
                val progress = seekBar?.progress ?: 0
                myMusicPlayer.seek(progress)
            }

        })
    }

    private fun pause() {
        myMusicPlayer.pause()
    }

    private fun resume() {
        myMusicPlayer.resume()
    }

    private fun open() {
        myMusicPlayer.setDataSource("${cacheDir}${File.separator}1.mp3")
        //myMusicPlayer.setDataSource("http://mpge.5nd.com/2015/2015-11-26/69708/1.mp3")
        myMusicPlayer.prepare()
    }


}